// webserver.c
#include "webserver.h"
#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/ip4_addr.h"

#include "flash.h"
#include "html.dashboard.h"
#include "html.wifiform.h"
#include "html.wifisuccess.h"

#define HTTP_PORT 80

// other responses defined n the html files
#define RESPOND_SSE "data: {\"rssi\":-70,\"trend\":\"STARTED\",\"uptime\":0}\r\r\n"

#define HEADER_CONTINUE "HTTP/1.1 100 Continue\r\n\r\n"
// this one needs to be modifiable to add length
#define HEADER_SUCCESS            \
    "HTTP/1.1 200 OK\r\n"         \
    "Content-Type: text/html\r\n" \
    "Connection: close\r\n"       \
    "Cache-Control: no-cache\r\n" \
    "Content-Length:%d\r\n\r\n"

#define HEADER_SSE                        \
    "HTTP/1.1 200 OK\r\n"                 \
    "Content-Type: text/event-stream\r\n" \
    "Cache-Control: no-cache\r\n"         \
    "Connection: keep-alive\r\n"          \
    "Access-Control-Allow-Origin: *\r\n\r\n"

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
bool parser(char *haystack, char *needle, char *destination)
{
    char *key = strstr(haystack, needle);
    if (key != NULL) // assign and check due to extra parenthises
    {
        char *value = strchr(key,'=')+1;
        int len = strcspn(value,"&\0");
        if (len ==0){
            printf("Found %s empty!\n",key);
        }
        else {
            strncpy(destination, value, len);
            printf("Found %s%s\n", needle,destination);
        }
        destination[len]='\0';
        return true;
    }
    return false;
}

static err_t http_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    static bool responding = false;
    char *header = NULL;
    char buffer[256];
    char *response = NULL;
    if (err != ERR_OK || p == NULL)
    {
        if (tpcb == sse_client)
            sse_client = NULL;

        tcp_close(tpcb);
        return ERR_OK;
    }

    tcp_recved(tpcb, p->tot_len);

    if (p->len == 0 || (strncmp(p->payload, "GET /f", 6) == 0))
    {
        tcp_close(tpcb);
        pbuf_free(p);
        return ERR_OK;
    }

    char req[1024];
    int length = 0;
    if (p->len > sizeof(req))
    {
        length = sizeof(req);
        printf("Request of %d truncated to %d prevent buf overflow.\n", p->len, length);
    }
    else
    {
        length = p->len;
    }

    memset(req, 0, sizeof(req));
    memcpy(req, p->payload, p->len);

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
    else if (responding || (length > 7 && (req, "POST / ", 7) == 0 || strncmp(req, "POST /", 6) == 0))
    {
        printf("Post:%s\n", req);
        responding = true;
        DeviceSettings *settings = load_settings();
        static bool ss,ps,bl=false;

         ss=parser(req, "_ssid=", settings->ssid);
         ps=parser(req, "_password=", settings->password);
         bl=parser(req, "_BLE_target=", settings->bleTarget);

        if (ss && ps && bl)
        {
            printf("Saving:%s\n%s\n%s\n%s\n",
                   settings->bleTarget,
                   settings->device_id,
                   settings->password,
                   settings->ssid);
            save_settings(); // Save immediately

            printf("RESPOND_SUCCESS\n");
            response = RESPOND_SUCCESS;
            responding = false;
        }
        else
        {
            pbuf_free(p);
            return ERR_OK;
        }
    }
    else if (length > 11 && strncmp(req, "GET /events", 11) == 0)
    {
        // SSE connection
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
    else if (length > 6 && strncmp(req, "GET / ", 6) == 0 || strncmp(req, "GET /", 5) == 0)
    {
        if (provisioning)
        {
            printf("RESPOND_FORM %15s\n", req);
            response = RESPOND_FORM;
        }
        else
        {
            // Serve dashboard
            printf("RESPOND_DASHBOARD\n");
            response = RESPOND_DASHBOARD;
        }
    }
    else
    {
        // 404 or ignore
        tcp_close(tpcb);
        pbuf_free(p);
        return ERR_ABRT;
    }

    int len = strlen(response);
    if (header == NULL)
    {
        snprintf(buffer, strlen(HEADER_SUCCESS) + 6, HEADER_SUCCESS, len);
        header = buffer;
    }

    cyw43_arch_lwip_begin();
    tcp_write(tpcb, header, strlen(header), TCP_WRITE_FLAG_COPY);

    if (response != NULL)
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
    printf(" http://%s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    return true;
}

void webserver_poll(void)
{
    cyw43_arch_poll();
}

void webserver_push_update(char *msg, size_t size)
{
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
        sse_client = NULL;
    }
    cyw43_arch_lwip_end();
}