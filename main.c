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
#include "secrets.h"
#include <time.h>

// #define DEBUG 1

bool GetTime(struct tm *lt)
{
    static char buffer[80];
    // This stores the Unix Time at the exact moment the Pico booted (Time 0)
    static time_t boot_unix_offset = 0;

    if (boot_unix_offset == 0)
    {
        time_t t_network_time;
        char zone[5];

        while (!networkGet("192.168.12.1", "//TMI//v1//gateway?get=time"))
        {
            touchBase();
        }
        while (!ResponseNumber("localTime", &t_network_time, 10))
        {
            touchBase();
        }

        while (!ResponseString("localTimeZone", zone, 5))
        {
            touchBase();
        }
        int tz_offset_seconds = atoi(zone) * 3600;

        // Current real-world time (adjusted for TZ)
        time_t adjusted_network_time = t_network_time + tz_offset_seconds;

        // Calculate what the Unix time was when the Pico was at 0ms uptime
        // to_ms_since_boot returns milliseconds; we divide by 1000 for seconds
        uint32_t uptime_seconds = to_ms_since_boot(get_absolute_time()) / 1000;
        boot_unix_offset = adjusted_network_time - uptime_seconds;

//        printf("Network Unix: %llu\n", t_network_time);
//        printf("Boot Offset: %llu\n", boot_unix_offset);
    }

    // Now, to get the current time, just add current uptime to our offset
    time_t now = boot_unix_offset + (to_ms_since_boot(get_absolute_time()) / 1000);

    localtime_r(&now, lt);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %I:%M:%S %p", lt);
    printf("Date/Time:\t%s\n", buffer);

    return true;
}

bool AssignTeslaBLE_Name(char *target)
{
    while (!network_post("{\"status\":\"ready\"}", AZURE_HOST, PHP_PATH))
    {
        printf("Retrying Tesla\n");
        touchBase();
    }
    while (!ResponseString("BLE", target, 32))
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

    struct tm now;
    GetTime(&now);

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
        //        GetTime(&now);
    }
    printf("Reseting to connect to %s.\n", local->ssid);
    reset();
}
