#include "wifi_provisioning.h"
#include "flash.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "hardware/watchdog.h"

#include "lwip/tcp.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include <string.h>
#include <stdio.h>
DeviceSettings New_settings = {0};

#define COUNT_DOWN_SECONDS "30"
#define RESET_TIME 150000 // 15s

static volatile bool provisioned = false;
static volatile bool connected = false;

static struct tcp_pcb *http_server_pcb = NULL;

static const char *HTML_FORM =
    "<!DOCTYPE html>"
    "<html><head><title>" CYW43_HOST_NAME
    " Setup</title>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<style>"
    "body{font-family:sans-serif;text-align:center;margin:40px;background:#f8f9fa;}"
    ".box{max-width:400px;margin:0 auto;background:white;padding:30px;border-radius:12px;box-shadow:0 4px 20px rgba(0,0,0,0.1);}"
    "input,button{width:100%;padding:12px;margin:10px 0;font-size:1.2em;box-sizing:border-box;}"
    "button{background:#0d6efd;color:white;border:none;border-radius:8px;cursor:pointer;}"
    "</style></head>"
    "<body>"
    "<div class=\"box\">"
    "<h1>" CYW43_HOST_NAME " Setup</h1>"
    "<p>Enter your home WiFi:</p>"
    "<form action=\"/\" method=\"post\">"
    "SSID:<br><input type=\"text\" name=\"s\" maxlength=\"32\" required><br>"
    "Password:<br><input type=\"password\" name=\"p\" maxlength=\"64\"><br><br>"
    "BLE Name:<br><input type=\"text\" name=\"n\" maxlength=\"32\"><br><br>"
    "<button type=\"submit\">Connect</button>"
    "</form>"
    "</div>"
    "</body></html>";

#define DASHBOARD "http://" CYW43_HOST_NAME

static const char *SUCCESS_HTML =
    "<!DOCTYPE html><html><head><title>Success</title>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<style>"
    "body{font-family:sans-serif;text-align:center;margin:40px;background:#f4f4f9;color:#333;}"
    ".timer{font-size:48px;font-weight:bold;color:#0078d4;margin:20px;}"
    ".loader{border:6px solid #f3f3f3;border-top:6px solid #0078d4;border-radius:50%;width:40px;height:40px;animation:spin 2s linear infinite;display:inline-block;}"
    "@keyframes spin{0%{transform:rotate(0deg);}100%{transform:rotate(360deg);}}"
    "</style></head>"
    "<body>"
    "<h1>Configuration Saved!</h1>"
    "<p>Connecting to your WiFi...<br>The AP will disappear if successful.</p>"
    "<div class=\"loader\"></div>"
    "<div class=\"timer\" id=\"countdown\">" COUNT_DOWN_SECONDS "</div>"
    "<p>Redirecting to dashboard in <span id=\"seconds\">" COUNT_DOWN_SECONDS "</span> seconds...</p>"
    "<script>"
    "var seconds = " COUNT_DOWN_SECONDS ";"
    "var countdown = document.getElementById('countdown');"
    "var secText = document.getElementById('seconds');"
    "var timer = setInterval(function() {"
    "seconds--;"
    "countdown.textContent = seconds;"
    "secText.textContent = seconds;"
    "if (seconds <= 0) {"
    "clearInterval(timer);"
    "window.location.href = '" DASHBOARD "';"
    "}"
    "}, 1000);"
    "</script>"
    "</body></html>";

static dhcp_server_t dhcp_srv;

static void start_dhcp_server(void)
{
    ip4_addr_t gw, mask;
    IP4_ADDR(&gw, 192, 168, 4, 1);     // Gateway / server IP
    IP4_ADDR(&mask, 255, 255, 255, 0); // Standard /24 netmask

    dhcp_server_init(&dhcp_srv, &gw, &mask);
    printf("DHCP server started\n");
}

static void stop_dhcp_server(void)
{
    dhcp_server_deinit(&dhcp_srv);
    printf("DHCP server stopped\n");
}

// Basic URL decode (handles + and %XX)
static void url_decode(char *dst, const char *src, size_t max_len)
{
    char *d = dst;
    while (*src && (d - dst) < max_len - 1)
    {
        if (*src == '+')
        {
            *d++ = ' ';
        }
        else if (*src == '%' && src[1] && src[2])
        {
            char hex[3] = {src[1], src[2], '\0'};
            *d++ = (char)strtol(hex, NULL, 16);
            src += 2;
        }
        else
        {
            *d++ = *src;
        }
        src++;
    }
    *d = '\0';
}

static void parse_post_value(const char *body, const char *key, char *output, size_t max_len)
{
    const char *key_pos = strstr(body, key);
    if (!key_pos)
    {
        output[0] = '\0';
        return;
    }

    const char *val_start = key_pos + strlen(key); // After '='
    const char *val_end = strchr(val_start, '&');
    if (!val_end)
        val_end = val_start + strlen(val_start);

    size_t len = val_end - val_start;

    // Assuming large lengths might be a memory leak
    if (len == 0 || len >= max_len)
    {
        output[0] = '\0';
        return;
    }

    char encoded[65];
    strncpy(encoded, val_start, len);
    encoded[len] = '\0';

    url_decode(output, encoded, max_len);
}

static err_t http_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{

    if (err != ERR_OK || p == NULL)
    {
        tcp_close(tpcb);
        return ERR_OK;
    }

    tcp_recved(tpcb, p->tot_len);

    char *data = (char *)p->payload;
    size_t len = p->tot_len;

    bool is_post = (len >= 6 && strncmp(data, "POST /", 6) == 0);

    const char *response = HTML_FORM;
    size_t response_len = strlen(HTML_FORM);

    if (is_post)
    {
        char *body = strstr(data, "\r\n\r\n");
        if (body)
        {
            body += 4;

            parse_post_value(body, "s=", New_settings.ssid, sizeof(New_settings.ssid));
            parse_post_value(body, "p=", New_settings.password, sizeof(New_settings.password));
            parse_post_value(body, "n=", New_settings.bleTarget, sizeof(New_settings.bleTarget));
            pico_get_unique_board_id_string(New_settings.device_id, 32);
            // Not configured
            if (strlen(New_settings.ssid) > 0)
            {
                response = SUCCESS_HTML;
                response_len = strlen(SUCCESS_HTML);
                provisioned = true;
            }
        }
    }

    char header[128];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
             response_len);

    tcp_write(tpcb, header, strlen(header), TCP_WRITE_FLAG_COPY);
    tcp_write(tpcb, response, response_len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    tcp_close(tpcb);
    pbuf_free(p);
    return ERR_OK;
}

static err_t http_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    if (err != ERR_OK || newpcb == NULL)
        return ERR_MEM;

    // Pass the settings pointer to the connection PCB
    //    tcp_arg(newpcb, arg);

    tcp_nagle_disable(newpcb);
    tcp_recv(newpcb, http_recv_cb);

    connected = true;
    return ERR_OK;
}

static void start_http_server()
{
    http_server_pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!http_server_pcb)
    {
        printf("Failed to allocate HTTP PCB\n");
        return;
    }

    err_t err = tcp_bind(http_server_pcb, IP_ADDR_ANY, 80);
    if (err != ERR_OK)
    {
        printf("HTTP bind failed: %d\n", err);
        tcp_abort(http_server_pcb);
        return;
    }

    http_server_pcb = tcp_listen_with_backlog(http_server_pcb, TCP_DEFAULT_LISTEN_BACKLOG);
    if (!http_server_pcb)
    {
        printf("HTTP listen failed\n");
        return;
    }

    tcp_accept(http_server_pcb, http_accept_cb);

    printf("HTTP server listening on port 80\n");
}

static void stop_http_server(void)
{
    if (http_server_pcb)
    {
        tcp_abort(http_server_pcb);
        http_server_pcb = NULL;
    }
}

bool wifi_provisioning_start()
{
    watchdog_enable(RESET_TIME, true);

    if (cyw43_arch_init())
    {
        printf("WiFi init failed\n");
        return false;
    }
    watchdog_update();

    char *ap_ssid = CYW43_HOST_NAME; //[24];
                                     //    snprintf(ap_ssid, sizeof(ap_ssid), CYW43_HOST_NAME "-%s", _settings->device_id + 8);

    printf("Starting AP: %s (Open)\n", ap_ssid);
    cyw43_arch_enable_ap_mode(ap_ssid, NULL, CYW43_AUTH_OPEN);
    watchdog_update();

    start_dhcp_server();
    dns_server_init();
    watchdog_update();

    printf("Browse to: http://%s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));

    start_http_server();
    watchdog_update();

    u8_t counter = 0;
    provisioned = false;
    while (!provisioned)
    {
        // Allow additional time to connect to web. If on website don't reset.
        if (connected || counter < 3)
        {
            watchdog_update();
            // count if not connected
            counter = counter + !connected;
        }
        cyw43_arch_poll();
        sleep_ms(10);
    }
    watchdog_update();
    printf("Saving:%s\n%s\n%s\n%s\n", 
        New_settings.bleTarget, 
        New_settings.device_id, 
        New_settings.password, 
        New_settings.ssid);
    save_settings(&New_settings); // Save immediately

    //    stop_http_server();
    //    stop_dhcp_server();
    //    dns_server_deinit();  // If you have it

    return true;
}