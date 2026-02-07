// webserver.c
#include "webserver.h"
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/ip4_addr.h"

#include "flash.h"
#include "html.dashboard.h"
#include "html.wifiform.h"
#include "html.wifisuccess.h"

#define HTTP_PORT 80
// #define DEBUG 1

// other responses defined n the html files
#define RESPOND_NOT_FOUND "<!DOCTYPE html><html><head><title>404 Not Found</title></head><body><h1>404 - File Not Found</h1></body></html>"
#define RESPOND_SSE "data: {\"trend\":\"STARTED\",\"uptime\":0}\r\r\n"
#define HEADER_CONTINUE "HTTP/1.1 100 Continue\r\n\r\n"
// this one needs to be modifiable to add length
#define HEADER_SUCCESS            \
    "HTTP/1.1 200 OK\r\n"         \
    "Content-Type: text/html\r\n" \
    "Connection: close\r\n"       \
    "Cache-Control: no-cache\r\n" \
    "Content-Length:%d\r\n\r\n"
#define HEADER_JSON                      \
    "HTTP/1.1 200 OK\r\n"                \
    "Content-Type: application/json\r\n" \
    "Connection: close\r\n"              \
    "Cache-Control: no-cache\r\n"        \
    "Content-Length:%d\r\n\r\n"
#define HEADER_SSE                        \
    "HTTP/1.1 200 OK\r\n"                 \
    "Content-Type: text/event-stream\r\n" \
    "Cache-Control: no-cache\r\n"         \
    "Connection: keep-alive\r\n"          \
    "Access-Control-Allow-Origin: *\r\n\r\n"
#define HEADER_REDIRECT                \
    "HTTP/1.1 302 Found\r\n"           \
    "Location: http://192.168.4.1\r\n" \
    "Connection: close\r\n"            \
    "Content-Length: %d\r\n\r\n"
#define HEADER_NOT_FOUND                         \
    "HTTP/1.1 404 Not Found\r\n"                 \
    "Content-Type: text/html; charset=utf-8\r\n" \
    "Content-Length: %d\r\n\r\n"

// SSE clients (simple single-client for now – extend to list if needed)
static struct tcp_pcb *sse_client = NULL;
// confusing global parameter needs to be eliminatged
static bool provisioning;

static err_t http_sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    // Close after send complete for non-SSE
    if (tpcb != sse_client)
    {
        tcp_close(tpcb);
    }
    return ERR_OK;
}

// Define this at the top of your file or in a header
int hex_char_to_int(char c)
{
    c = tolower((unsigned char)c);
    if (isdigit(c))
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return 0;
}

void url_decode(char *dst, const char *src)
{
    char *p_dst = dst;
    const char *p_src = src;

    while (*p_src)
    {
        if (*p_src == '%' && isxdigit(p_src[1]) && isxdigit(p_src[2]))
        {
            // Take the two hex characters and convert them
            int high = hex_char_to_int(p_src[1]);
            int low = hex_char_to_int(p_src[2]);

            // Combine them into a single byte (high nibble | low nibble)
            *p_dst++ = (char)((high << 4) | low);
            p_src += 3;
        }
        else if (*p_src == '+')
        {
            *p_dst++ = ' ';
            p_src++;
        }
        else
        {
            *p_dst++ = *p_src++;
        }
    }
    *p_dst = '\0'; // Crucial: Terminate the string to remove the "tail"
}
// Remove url_decode from here!
bool parser(char *haystack, const char *needle, char *destination, size_t dest_max_len)
{
    char *ptr = haystack;
    size_t needle_len = strlen(needle);

    while ((ptr = strstr(ptr, needle)) != NULL)
    {
        bool is_start = (ptr == haystack || *(ptr - 1) == '?' || *(ptr - 1) == '&');

        if (is_start && ptr[needle_len] == '=')
        {
            char *value = ptr + needle_len + 1;
            int len = strcspn(value, "&\0");
            if (len == 0)
            {
                destination[0] = '\0';
            }
            else
            {
                size_t copy_len = (len < dest_max_len - 1) ? len : dest_max_len - 1;
                strncpy(destination, value, copy_len);
                destination[copy_len] = '\0';
            }
            return true;
        }
        ptr++;
    }
    return false;
}

static err_t http_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    static bool responding = false;
    char *header;
    static char header_buffer[256];
    char *response;
    static char response_buffer[256];
    static char req[1465];
    // char req[256];
    // use static buffer to prevent mem allocation but assure they are empty strings at start
    response = response_buffer;
    header = header_buffer;
    response_buffer[0] = '\0';
    header_buffer[0] = '\0';
    req[0] = '\0';

    if (p == NULL)
        return ERR_BUF;

    if (err != ERR_OK)
    {
        printf("HTTP error:%d\n", err);
        if (tpcb == sse_client)
            sse_client = NULL;

        tcp_close(tpcb);
        return ERR_RST;
    }

    tcp_recved(tpcb, p->tot_len);

    int length = MIN(p->len, sizeof(req) - 1);
    if (p->len > sizeof(req) - 1)
    {
        printf("Request of %d length truncated to %d prevent buf overflow.\n", p->len, length);
    }
    memcpy(req, p->payload, length);
    req[length] = '\0';
    // fragmented
    if (p->tot_len != p->len)
    {
        printf("!!!!!!!!!!!!!!!!!Fragmentation detected but has not been coded!!!!!!!!!!!!!\n");
        //        printf("Next Total Length:%d, Length:%d\n", p->next->tot_len, p->next->len);
        //        printf("next:%s\n", p->next->payload);
    }
    else if (length > 12 && strstr(req, "100 Continue") != NULL)
    {
        header = HEADER_CONTINUE;
    }
    else if (responding || (length > 6 && strncmp(req, "POST /", 6) == 0))
    {
        char posted[256];
        char *body = responding ? req : strstr(req, "\r\n\r\n") + 4;
        responding = true;

        // 1. Decode the whole request buffer ONCE
        url_decode(posted, body);

        bool ss, pw, bt = false;
        // 2. Now parse the keys from the clean, decoded string. If you find anyof them save.
        ss = parser(posted, "_ssid", settings.ssid, 32);
        pw = parser(posted, "_password", settings.password, 64);
        bt = parser(posted, "_bleName", settings.bleName, 32);

        if (ss && pw && bt)
        {
            //            memset(settings.bleAddress, 0xFF, sizeof(settings.bleAddress));
            responding = false;
            settings.bleAddress[0] = 0xFF;
            settings.bleAddress[1] = 0xFF;
            settings.bleAddress[2] = 0xFF;
            settings.bleAddress[3] = 0xFF;

            save_settings(true); // Save (don't blank) immediately
            if (provisioning)
            {
                provisioning = false;
                response = RESPOND_SUCCESS;
            }
            else
            {
                response = RESPOND_DASHBOARD;
            }
        }
        else
        {
            //            tcp_close(tpcb);//this was previously commented out
            pbuf_free(p);
            return ERR_OK;
        }
    }
    else if (length > 12 && strncmp(req, "GET /events ", 12) == 0)
    {
        // SSE connection - single client
        if (sse_client && sse_client != tpcb)
        {
            tcp_abort(sse_client);
        }
        sse_client = tpcb;

        // Send immediate initial data to validate connection and prevent empty parse error
        response = RESPOND_SSE;
        header = HEADER_SSE;
        printf("SSE client connected – sent initial data\n");
    }
    else if ((length > 14 && (strncmp(req, "GET /settings ", 14) == 0)))
    {
        printf("Serving Settings\n");
        load_settings();
        snprintf(response_buffer, sizeof(response_buffer),
                 "{\"ssid\":\"%s\",\"password\":\"%s\",\"bleName\":\"%s\"}\r\r\n",
                 "", "", "");
        header = HEADER_JSON;
        response = response_buffer;
    }
    else if ((length > 9 && (strncmp(req, "GET /pio=", 9) == 0)))
    {
        int pio = atoi(req + 9);
        if (strstr(req, "toggle"))
        {
            pin(pio, 1);
        }
        if (strstr(req, "press"))
        {
            pin(pio, 1);
            cyw43_delay_ms(500);
            pin(pio, 1);
        }

        //        printf("RESPOND_PIO\n");
        header = HEADER_JSON;

        snprintf(response_buffer, sizeof(response_buffer),
                 "{\"pio\":\"%d\",\"value\",%s}\r\r\n", pio, (pin(pio, false) ? "true" : "false"));
        response = response_buffer;
    }
    else if ((length > 15 && (strncmp(req, "GET /dashboard ", 15) == 0)))
    {
        //       printf("RESPOND_DASHBOARD\n");
        response = RESPOND_DASHBOARD;
    }
    else if (length > 12 && (strncmp(req, "GET /config ", 12) == 0))
    {
        //       printf("RESPOND_FORM\n");
        response = RESPOND_FORM;
    }
    else if (length > 11 && (strncmp(req, "GET /stats ", 11) == 0))
    {

        header = HEADER_JSON;
        snprintf(response_buffer, sizeof(response_buffer),
                 "{\"key\":\"%s\",\"value\",%s}\r\r\n", "Temporary", "true");
        response = response_buffer;
    }
    else if (length > 6 || (strncmp(req, "GET / ", length) == 6))
    {
        if (provisioning)
        {
            if (strnstr(req, "Host: captive.apple.com", length))
            {
                printf("Redirecting to provisioning\n");
                header = HEADER_REDIRECT;
            }
            else
            {
                header = HEADER_SUCCESS;
                //          printf("RESPOND_FORM\n");
                response = RESPOND_FORM;
            }
        }
        else
        {
            // Serve dashboard
            header = HEADER_SUCCESS;
            //           printf("RESPOND_DASHBOARD\n");
            response = RESPOND_DASHBOARD;
        }
    }
    else
    {
        header = HEADER_NOT_FOUND;
        response = RESPOND_NOT_FOUND;
    }
    if (header == NULL || strlen(header) == 0)
    {
        header = HEADER_SUCCESS;
    }
    if (strstr(header, "Content-Length:") != 0)
    {
        snprintf(header_buffer, strlen(header) + 6, header, strlen(response));
        header = header_buffer;
    }
#ifdef DEBUG
    printf("Request:%.30s\n", req);
    printf("Header write:%.30s\n", header);
    printf("Response write:%.30s\n", response);
#endif
    cyw43_arch_lwip_begin();
    tcp_write(tpcb, header, strlen(header), TCP_WRITE_FLAG_COPY);

    if (response != NULL && strlen(response) > 0)
    {
        tcp_write(tpcb, response, strlen(response), TCP_WRITE_FLAG_COPY);
    }

    tcp_output(tpcb);
    cyw43_arch_lwip_end();
    return ERR_OK;
}

static err_t http_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    if (err != ERR_OK || newpcb == NULL)
        return ERR_MEM;

    tcp_recv(newpcb, http_recv_cb);
    tcp_nagle_disable(newpcb);
    return ERR_OK;
}

static struct tcp_pcb *listen_pcb = NULL;

bool webserver_init(bool _provisioning)
{
    static bool running = false;
    if (running)
    {
        return true;
    }
    running = true;
    provisioning = _provisioning;
    listen_pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!listen_pcb)
        return false;

    err_t err = tcp_bind(listen_pcb, IP_ADDR_ANY, HTTP_PORT);
    if (err != ERR_OK)
    {
        printf("Bind failed: %d\n", err);
        return false;
    }

    listen_pcb = tcp_listen_with_backlog(listen_pcb, TCP_DEFAULT_LISTEN_BACKLOG);
    if (!listen_pcb)
        return false;

    tcp_accept(listen_pcb, http_accept_cb);
    printf("http://%s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    return true;
}

void webserver_poll(void)
{
    cyw43_arch_poll();
}

void webserver_push_update(char *msg, size_t size)
{
#ifdef DEBUG
    printf("message:%s\n", msg);
#endif
    // No client or no data to send
    if (!sse_client || size == 0)
        return; // Nothing to send
    cyw43_arch_lwip_begin();
    if (tcp_sndbuf(sse_client) >= size)
    {
        tcp_write(sse_client, msg, size, TCP_WRITE_FLAG_COPY);
        tcp_output(sse_client);
    }
    else
    {
        // Buffer full – drop client
        printf("Dropped SSE\n");
        sse_client = NULL;
    }
    touchBase();

    cyw43_arch_lwip_end();
}