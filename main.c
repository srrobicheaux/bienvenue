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

#define DEBUG 1

bool AssignTeslaBLE_Name(char *target)
{
    // 2. Fetch BLE Name from Tesla
    network_post("{\"status\":\"ready\"}");
    while (strlen(target) == 0)
    {
        cyw43_arch_poll();
        NetworkResponse("BLE", target, 32);
        touchBase();
    }
}
int main()
{
    stdio_init_all();
    sleep_ms(2000); // Give serial time to open
    bool connected = false;
    load_settings();

#ifdef DEBUG
    printf("Debugging mode enabled\n");
#endif

    char id_str[32];
    pico_get_unique_board_id_string(id_str, sizeof(id_str));
    printf("%s Starting on Pico ID: %s\n", CYW43_HOST_NAME, id_str);

    //    strcpy(settings.bleTarget,"LE-Bose Revolve+ SoundLink");
    //    strcpy(settings.bleTarget,"N169A");
    touchBase();
    printf("Current Config:\tTarget:%s\tSSID:%s\n", settings.bleTarget, settings.ssid);
    sleep_ms(100);
    if (settings.initialized)
    {
        connected = ConnectNetwork(&settings);
        touchBase();
    }
    if (!connected)
    {
        settings.initialized = false;
        wifi_provisioning_start();
    }
    touchBase();

    webserver_init(!connected); // Toggle responses based on AP or Wifi COnnection
    touchBase();

    if (connected && strlen(settings.bleTarget) < 2)
    {
        AssignTeslaBLE_Name(settings.bleTarget);
        touchBase();
    }
    BLE_Init(settings.bleTarget, webserver_push_update);
    touchBase();
    radar_init();
    touchBase();
    Detection det = {0};

    while (connected || !settings.initialized)
    {
        cyw43_bluetooth_hci_process(); // Your BLE work
        webserver_poll();              // Handle webserver lwIP worknsive
        radar_run_cycle();
        process_one_beam(CHIRP_LENGTH, &det);
        touchBase();
    }
    reset();
}
