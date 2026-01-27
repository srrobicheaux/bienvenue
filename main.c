#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "pico/cyw43_arch.h"

#include "ble_strength.h"
#include "webserver.h"
#include "webclient.h"
#include "wifi_provisioning.h"
#include "dhcpserver.h"
#include "flash.h"
#include "ultrasonic.h"
#include "malloc.h"

// #define DEBUG 1

bool AssignTeslaBLE_Name(char *target)
{
    while (!network_post("{\"status\":\"ready\"}"))
    {
        printf("Retrying Tesla\n");
        touchBase();
    }
    while (!NetworkResponse("BLE", target, 32))
    {
        touchBase();
    }
    printf("Tesla BLE Name:\t%s\n", target);
}
int main()
{
    stdio_init_all();
    sleep_ms(2000); // Give serial time to open
    DeviceSettings *local = load_settings();
    bool connected = ConnectNetwork(local);
    if (!connected)
    {
        wifi_provisioning_start();
    }
    touchBase();
    webserver_init(!connected); // Toggle responses based on AP or Wifi COnnection
    touchBase();

    if (connected && strlen(local->bleName) < 2)
    {
        printf("Contacting Tesla\n");
        AssignTeslaBLE_Name(local->bleName);
        touchBase();
    }

    BLE_Init(webserver_push_update);
    touchBase();

    radar_init();
    touchBase();
    Detection det = {0};

    while (connected || strlen(settings.ssid) == 0)
    {
        cyw43_bluetooth_hci_process();
        webserver_poll();
        radar_run_cycle();
        process_one_beam(CHIRP_LENGTH, &det, webserver_push_update);

        static int debug_ultra = 0;
        if (++debug_ultra % 10 == 0)
        {
            send_status_event(webserver_push_update);
        }
            touchBase();
    }
    printf("Reseting to connect to %s.\n", local->ssid);
    reset();
}
