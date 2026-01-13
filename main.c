#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"
#include "ble_strength.h"
#include "webserver.h"
#include "webclient.h"
#include "wifi_provisioning.h"
#include "dhcpserver.h"
#include "flash.h"

#define RESET_TIME 15000000 // 150000 // 15s

bool blink = false;
void reset()
{
    watchdog_enable(10, true); // true = reset on timeout
    while (1)
        ;
}
void ButtonPress()
{
    watchdog_disable();
    uint32_t LongPress = get_absolute_time() + 10000000; //~10seconds

    while (get_bootsel_button())
        ;
    // if more than ~10s held, reset settings
    if (LongPress < get_absolute_time())
    {
        // blank id to indicate uninitialized
        save_settings(NULL);
        printf("Button held long enough, resetting settings.\n");
    }
    else
    {
        printf("Short Button press, rebooting only.\n");
    }
    reset();
}

void touchBase()
{
    blink = !blink;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, blink);
    watchdog_update();

    // Button Pressed: either reset settings or just reboot
    if (get_bootsel_button())
    {
        printf("Button Pressed! System will reboot. Hold for 15 seconds for flash reset.\n");
        ButtonPress();
    }
}

bool ConnectNetwork(DeviceSettings *settings)
{
    if (settings->ssid[0] == '\0')
    {
        printf("No SSID configured.\n");
        return false;
    }
    if (cyw43_arch_init())
    {
        printf("WiFi init failed\n");
        reset();
    }
    touchBase();
    cyw43_arch_enable_sta_mode();
    touchBase();

    int error = cyw43_arch_wifi_connect_timeout_ms(settings->ssid, settings->password, CYW43_AUTH_WPA3_WPA2_AES_PSK, 15000);
    if (error)
    {
        printf("Failed to connect to %s. Error# %d\n", settings->ssid, error);
        cyw43_arch_deinit();
        return false;
    }
    else
    {
        printf("Connected to %s @  ", settings->ssid, ip4addr_ntoa(netif_ip4_addr(netif_default)));
        return true;
    }
    touchBase();
}
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
    DeviceSettings settings;

    stdio_init_all();
    sleep_ms(2000); // Give serial time to open

    char id_str[32];
    pico_get_unique_board_id_string(id_str, sizeof(id_str));
    printf("%s Starting on Pico ID: %s\n", CYW43_HOST_NAME, id_str);

    // device id is used as a magic key for flash and network
    if (load_settings(&settings, id_str))
    {
        watchdog_enable(RESET_TIME, true);
        printf("Button until connected to reset!\n");
        printf("Current Config:\tTarget:%s\tSSID:%s\n", settings.bleTarget, settings.ssid);
        if (ConnectNetwork(&settings))
        {
            // can't touchbase until after web init because of LED Blink Do'nt forget again!
            touchBase();
            webserver_init(NULL);//Settings are not needed
            touchBase();
            if (strlen(settings.bleTarget) == 0)
            {
                AssignTeslaBLE_Name(settings.bleTarget);
                touchBase();
            }
            BLE_Init(&settings, webserver_push_update);
            touchBase();
            int once = 1;
            while (true)
            {
                if (once && strchr(settings.bleTarget, ':'))
                {
                    printf("Save the MAC once discovered:%s\n", settings.bleTarget);
                    save_settings(&settings);
                    touchBase();

                    once = 0;
                }
                cyw43_bluetooth_hci_process(); // Your BLE work
                webserver_poll();              // Handle webserver lwIP worknsive
                // Low-power sleep if no immediate work (safe with poll above)
                touchBase();
//                sleep_ms(10); // Short sleep – keeps respo
            }
            reset();
        }
    }
    if (wifi_provisioning_start())
    {
        printf("Sucessfully Provisioned. Restarting.\n");
    }
    // allways try wifi after Provisioning or after a long time retry Wifi incase it just went down
    reset();
}
