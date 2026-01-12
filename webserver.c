// webserver.c
#include "webserver.h"
#include <malloc.h>

#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/ip4_addr.h"

#include <string.h>
#include <stdio.h>

static const char *DASHBOARD_HTML =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"Connection: close\r\n"
"Cache-Control: no-cache\r\n\r\n"
#include "dashboard.html.h"
;

// SSE clients (simple single-client for now – extend to list if needed)
static struct tcp_pcb *sse_client = NULL;

#define HTTP_PORT 80

static err_t http_sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    // Close after send complete for non-SSE
    if (tpcb != sse_client)
    {
        tcp_close(tpcb);
    }
    return ERR_OK;
}

static err_t http_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (err != ERR_OK || p == NULL)
    {
        if (tpcb == sse_client)
            sse_client = NULL;
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;

    if (strncmp(req, "GET /events", 11) == 0)
    {
        // SSE connection
        if (sse_client && sse_client != tpcb)
        {
            tcp_abort(sse_client); // Only one SSE client
        }
        sse_client = tpcb;

        const char *sse_header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n";

        tcp_write(tpcb, sse_header, strlen(sse_header), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);

        // Send immediate initial data to validate connection and prevent empty parse error
        char init_payload[128];
        snprintf(init_payload, sizeof(init_payload),
            "data: {\"rssi\":-70,\"trend\":\"STARTED\",\"memory\":%d,\"uptime\":%u}\r\r\n",
                     mallinfo().fordblks,get_absolute_time());

        cyw43_arch_lwip_begin();
        tcp_write(tpcb, init_payload, strlen(init_payload), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
        cyw43_arch_lwip_end();

        printf("SSE client connected – sent initial data\n");
    }
    else if (strncmp(req, "GET / ", 6) == 0 || strncmp(req, "GET /", 5) == 0)
    {
        // Serve dashboard
        tcp_write(tpcb, DASHBOARD_HTML, strlen(DASHBOARD_HTML), TCP_WRITE_FLAG_COPY);
        tcp_sent(tpcb, http_sent_cb);
        tcp_output(tpcb);
    }
    else
    {
        // 404 or ignore
        tcp_close(tpcb);
    }

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
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

bool webserver_init(void)
{
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
    //No client or no data to send
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