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
    if (cyw43_arch_init())
    {
        printf("WiFi init failed\n");
        reset();
    }
    char *ap_ssid = CYW43_HOST_NAME; //[24];
    printf("Starting AP: %s (Open)\n", ap_ssid);
    cyw43_arch_enable_ap_mode(ap_ssid, NULL, CYW43_AUTH_OPEN);

    start_dhcp_server();
    dns_server_init();

    printf("Provision url:\t");
    return true;
}

bool ConnectNetwork(DeviceSettings *pro_settings)
{
    if (pro_settings->ssid[0] == '\0')
    {
        printf("SSID:\tNot configured\n");
        return false;
    }
    if (cyw43_arch_init())
    {
        printf("WiFi init failed\n");
        reset();
    }
    cyw43_arch_enable_sta_mode();

    int error = cyw43_arch_wifi_connect_timeout_ms(pro_settings->ssid, pro_settings->password, CYW43_AUTH_WPA3_WPA2_AES_PSK, 30000);
//    int error = cyw43_arch_wifi_connect_timeout_ms("ROBO", "UseTheForce", CYW43_AUTH_WPA3_WPA2_AES_PSK, 25000);
    if (error)
    {
        touchBase();
        cyw43_arch_deinit();
        printf("Failed to connect to %s. Error# %d\n", pro_settings->ssid, error);
        return false;
    }
    else
    {
        printf("WiFi Connect:\t", pro_settings->ssid);
        return true;
    }
}