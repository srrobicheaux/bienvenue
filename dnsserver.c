#include <stdint.h>
#include <string.h>
#include "pico/stdio.h"
#include "lwip/udp.h"
#include "lwip/prot/dns.h"

#define DNS_PORT 53

static struct udp_pcb *dns_pcb;

// The DNS Reply Header + Answer for "Everything points to 192.168.4.1"
static void dns_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (p->len < 12) { pbuf_free(p); return; }

    // Create response buffer
    struct pbuf *res = pbuf_alloc(PBUF_TRANSPORT, 64, PBUF_RAM);
    if (!res) { pbuf_free(p); return; }
    memset(res->payload, 0, res->tot_len);

    uint8_t *req = (uint8_t *)p->payload;
    uint8_t *ans = (uint8_t *)res->payload;

    // --- Header ---
    memcpy(ans, req, 2);      // ID: Copy from request
    ans[2] = 0x81; ans[3] = 0x80; // Flags: Standard query response, No error
    ans[4] = 0x00; ans[5] = 0x01; // Questions: 1
    ans[6] = 0x00; ans[7] = 0x01; // Answer RRs: 1

    // --- Copy Question Section ---
    int ptr = 12;
    while (req[ptr] != 0 && ptr < p->len) ptr++; // Find end of name
    ptr += 5; // Include null byte, type (2), and class (2)
    memcpy(&ans[12], &req[12], ptr - 12);

    // --- Answer Section ---
    uint8_t *ans_ptr = &ans[ptr];
    *ans_ptr++ = 0xc0; *ans_ptr++ = 0x0c; // Pointer to domain name (offset 12)
    *ans_ptr++ = 0x00; *ans_ptr++ = 0x01; // Type: A (IPv4)
    *ans_ptr++ = 0x00; *ans_ptr++ = 0x01; // Class: IN
    *ans_ptr++ = 0x00; *ans_ptr++ = 0x00; // TTL: 60 seconds
    *ans_ptr++ = 0x00; *ans_ptr++ = 0x3c;
    *ans_ptr++ = 0x00; *ans_ptr++ = 0x04; // Data length: 4 bytes
    
    // The IP Address: 192.168.4.1
    *ans_ptr++ = 192; *ans_ptr++ = 168; *ans_ptr++ = 4; *ans_ptr++ = 1;

    // Send the response back to the requester
    udp_sendto(pcb, res, addr, port);
    
    pbuf_free(res);
    pbuf_free(p);
}

void dns_server_init() {
    dns_pcb = udp_new();
    udp_bind(dns_pcb, IP_ADDR_ANY, DNS_PORT);
    udp_recv(dns_pcb, dns_recv, NULL);
    printf("DNS Redirector initialized (Port 53)\n");
}