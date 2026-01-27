#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/api.h"
#include "pico/unique_id.h"
#include "secrets.h"

// Configuration
#define MIDDLEWARE_HOST AZURE_HOST
#define MIDDLEWARE_PATH PHP_PATH
#define MIDDLEWARE_PORT 80

// State structure to keep track of the request
typedef struct
{
    char request_buffer[1024]; // Buffer for headers + body
    struct tcp_pcb *pcb;
} post_state_t;

char *json_response; // Buffer for headers + body
bool NetworkResponse(const char *key, char *out_buf, size_t buf_size)
{
    if (json_response == NULL || out_buf == NULL || buf_size == 0)
    {
        return false;
    }
    // 1. Search for the key with quotes to avoid partial matches
    // We look for "key"
    char search_key[15];
    snprintf(search_key, sizeof(search_key), "\"%s\":\"", key);

    char *begining = strstr(json_response, search_key);
    if (!begining)
        return false;
    //shift past "key":"
    begining=begining+strlen(search_key);
    // Find the end
    char *end = strchr(begining , '\"');
    if (!end)
        return NULL;
    int len = end - begining;
    memcpy(out_buf, begining, len );
    out_buf[len] = '\0';
    return begining;
}

// --- Callback: Connection Closed ---
static void close_connection(struct tcp_pcb *pcb, post_state_t *state)
{
    if (pcb)
    {
        tcp_arg(pcb, NULL);
        tcp_sent(pcb, NULL);
        tcp_recv(pcb, NULL);
        tcp_err(pcb, NULL);
        tcp_close(pcb);
    }
    if (state)
        free(state);
    printf("Response Received\n");
}

// --- Callback: Data Received (The Response) ---
static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    post_state_t *state = (post_state_t *)arg;
    if (p == NULL)
    {
        close_connection(pcb, state);
        return ERR_OK;
    }

    tcp_recved(pcb, p->tot_len);
    printf("resp:%s\n",p->payload);

    // Create a temporary null-terminated string from the packet
    // We allocate +1 for the null terminator
    char *data = (char *)malloc(p->len + 1);
    if (data)
    {
        memcpy(data, p->payload, p->len);
        data[p->len] = '\0';

        // Find the start of the JSON (the first '{')
        char *json_start = strchr(data, '{');
        char *json_end = strrchr(data, '}');

        if (json_start && json_end && json_end > json_start)
        {
            // Null terminate at the end of the JSON to isolate it
            *(json_end + 1) = '\0';
            json_response = strdup(json_start);
        }
        free(data);
    }

    pbuf_free(p);
    return ERR_OK;
}

// --- Callback: Connected to Server ---
static err_t on_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
    post_state_t *state = (post_state_t *)arg;

    if (err != ERR_OK)
    {
        printf("Connection failed: %d\n", err);
        close_connection(pcb, state);
        return err;
    }

    //printf("Connected! Sending POST...\n");

    // Setup receive callback
    tcp_recv(pcb, on_recv);

    // Send the data we prepared in the state buffer
    // TCP_WRITE_FLAG_COPY tells lwIP to copy data so we can free our struct later if needed
    err_t write_err = tcp_write(pcb, state->request_buffer, strlen(state->request_buffer), TCP_WRITE_FLAG_COPY);

    if (write_err == ERR_OK)
    {
        tcp_output(pcb); // Flush
    }
    else
    {
        printf("Failed to write data: %d\n", write_err);
        close_connection(pcb, state);
    }

    return ERR_OK;
}

// --- 1. The Helper that actually connects (moved out of the main function) ---
static bool do_tcp_connect(ip_addr_t *ipaddr, post_state_t *state)
{
    state->pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!state->pcb)
    {
        printf("Failed to create PCB\n");
        free(state);
        return false;
    }

    tcp_arg(state->pcb, state);

    // Connect using the resolved IP
    err_t err = tcp_connect(state->pcb, ipaddr, MIDDLEWARE_PORT, on_connected);

    if (err != ERR_OK)
    {
        printf("TCP connect failed: %d\n", err);
        close_connection(state->pcb, state);
        return false;
    }

    return true;
}

// --- 2. The DNS Callback (Fires later if lookup was needed) ---
static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    post_state_t *state = (post_state_t *)callback_arg;

    if (ipaddr != NULL)
    {
        printf("DNS Resolved:\t%s -> %s\n", name, ipaddr_ntoa(ipaddr));
        // IP found! Now we connect.
        do_tcp_connect((ip_addr_t *)ipaddr, state);
    }
    else
    {
        printf("DNS Resolution Failed for %s\n", name);
        free(state); // Clean up memory since we can't send
    }
}

// --- 3. The Main Function ---
bool network_post(const char *json_body)
{
    char auth_key[32];
    pico_get_unique_board_id_string(auth_key, sizeof(auth_key));

    post_state_t *state = malloc(sizeof(post_state_t));
    if (!state)
    {
        printf("OOM\n");
        return false;
    }

    // Prepare buffer (same as before)...
    snprintf(state->request_buffer, sizeof(state->request_buffer),
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/json\r\n"
             "X-Auth-Key: %s\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             MIDDLEWARE_PATH, MIDDLEWARE_HOST, auth_key, strlen(json_body), json_body);

    ip_addr_t server_ip;

    // START DNS LOOKUP
    // We pass 'dns_found_cb' as the function to call when finished.
    // We pass 'state' as the argument so the callback has access to your data.
    err_t err = dns_gethostbyname(MIDDLEWARE_HOST, &server_ip, dns_found_cb, state);

    if (err == ERR_OK)
    {
        // Case A: IP was already in cache. Connect immediately.
        return do_tcp_connect(&server_ip, state);
    }
    else if (err == ERR_INPROGRESS)
    {
        // Case B: Lookup started. usage continues...
        // The 'dns_found_cb' will handle the rest later.
//        printf("Resolving hostname %s...\n", MIDDLEWARE_HOST);
        return true;
    }
    else
    {
        // Case C: Error (e.g. DNS not initialized)
        printf("DNS call failed: %d\n", err);
        free(state);
        return false;
    }
}
