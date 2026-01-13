#include "wifi_provisioning.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/watchdog.h"
#include "webserver.h"
#include "flash.h"

#include "lwip/tcp.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include <string.h>
#include <stdio.h>

#define RESET_TIME 15000000 * 100

//confusing global parameter needs to be eliminatged
bool is_provisioning;

DeviceSettings New_settings = {0};

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

bool wifi_provisioning_start()
{
    is_provisioning= true;

//    watchdog_enable(RESET_TIME, true);

    if (cyw43_arch_init())
    {
        printf("WiFi init failed\n");
        return false;
    }
    watchdog_update();

    char *ap_ssid = CYW43_HOST_NAME; //[24];
    printf("Starting AP: %s (Open)\n", ap_ssid);
    cyw43_arch_enable_ap_mode(ap_ssid, NULL, CYW43_AUTH_OPEN);
    watchdog_update();

    start_dhcp_server();
    dns_server_init();
    watchdog_update();

    printf("Provision System at:");
    webserver_init(true); // provisioning is true
    watchdog_update();

    u8_t counter = 0;
    //last param to be sent
    while (true)
    {
        watchdog_update();
        cyw43_arch_poll();
        sleep_ms(10);
    }
    watchdog_update();
 
    //    stop_http_server();
    //    stop_dhcp_server();
    //    dns_server_deinit();  // If you have it

    return true;
}