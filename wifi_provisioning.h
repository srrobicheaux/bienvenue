#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#include <stdbool.h>
#include "flash.h"
/**
 * Starts WiFi provisioning mode.
 * 
 * - Starts Access Point with unique SSID.
 * - cyw43 driver automatically provides DHCP (192.168.4.x range, gateway 192.168.4.1).
 * - Hosts captive-portal-friendly web server on 192.168.4.1:80.
 * - Any HTTP request gets the config form (triggers captive portal on most devices).
 * - On POST submit, captures credentials, returns them.
 * 
 * Caller saves Settings and connects (or reboots).
 * 
 * @return true if credentials captured (out buffers filled).
 */
bool wifi_provisioning_start();
bool ConnectNetwork(DeviceSettings *settings);

#endif