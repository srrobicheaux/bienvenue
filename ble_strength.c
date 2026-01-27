#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"
#include "ad_parser.h"
#include "flash.h"
#include "regression.h"

#include <stdio.h>
#define DEBUG 1

#define OUT_OF_GARAGE_DB -75
#define MAX_AGE_UNSEEN 5000000 // 5 seconds in microseconds
#define TRIGGER_DB .3
//  #define TRIGGER_DB .2
// #define MIN_TREND 1000000

// #define MIN_TREND 100

const void (*notify)(char *json, size_t size);

static btstack_packet_callback_registration_t hci_event_callback_registration;

bool bleAddressSet()
{
 return (settings.bleAddress[0] + settings.bleAddress[1] + settings.bleAddress[2] +settings.bleAddress[3]);
}

int DiscoveryName(uint8_t *packet, char *device_name, size_t size)
{
    // CASE 2: No MAC yet, searching by Name
    uint8_t adv_data_len = gap_event_advertising_report_get_data_length(packet);
    const uint8_t *adv_data = gap_event_advertising_report_get_data(packet);
    ad_context_t it;

    for (ad_iterator_init(&it, adv_data_len, adv_data); ad_iterator_has_more(&it); ad_iterator_next(&it))
    {
        uint8_t type = ad_iterator_get_data_type(&it);
        uint8_t len = ad_iterator_get_data_len(&it);
        const uint8_t *data = ad_iterator_get_data(&it);

        if (type == BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME || type == BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME)
        {
            int length = MIN(size - 1, len);
            strncpy(device_name, data, length);
            device_name[length] = '\0';
            return length;
        }
    }
    return 0;
}

// --- BLE Callback ---
void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    static char payload[256];
    static char name[64];

    if (packet_type != HCI_EVENT_PACKET)
        return;

    if (hci_event_packet_get_type(packet) == GAP_EVENT_ADVERTISING_REPORT)
    {
        bd_addr_t address;
        gap_event_advertising_report_get_address(packet, address);
        int8_t rssi = gap_event_advertising_report_get_rssi(packet);

        name[0] = '\0';
        if (!bleAddressSet())
        {
            // see if name is actually a MAC address and if so use it.
            if (DiscoveryName(packet, name, 64))
            {
                if (strcasecmp(name, settings.bleName) == 0)
                {
                    printf("Found %s at MAC:%s\n", settings.bleName, bd_addr_to_str(address));
                    bd_addr_copy(settings.bleAddress, address);
                    save_settings();
                }
            }
        }

        if (!bleAddressSet() || (bd_addr_cmp(address, settings.bleAddress) == 0))
        {
            float trend = get_gaussian_trend(rssi);
            //            float trend = 0;

            snprintf(payload, sizeof(payload),
                     "data: {\"type\":\"BLE\",\"MAC\":\"%s\",\"rssi\":%d,\"time_s\":%llu,\"trend\":\"%f\",\"name\":\"%s\"}\r\r\n",
                     bd_addr_to_str(address), rssi, get_absolute_time(), trend,
                     bleAddressSet() ? settings.bleName : name);
            notify(payload, strlen(payload));
        }
    }
}

bool BLE_Init(void (*notifer)(char *json, size_t size))
{
    // Initialize BTstack (Standard boiler plate)
    notify = notifer;
#ifdef DEBUG
    printf("BLE Target:\t%s.\n", (bleAddressSet()) ? bd_addr_to_str(settings.bleAddress) : settings.bleName);
    printf("BLE MAC:\t%s.\n", bd_addr_to_str(settings.bleAddress));
#endif

    l2cap_init();
    le_device_db_init();
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    hci_power_control(HCI_POWER_ON);
    // gap_set_local_name(CYW43_HOST_NAME);
    gap_set_scan_duplicate_filter(0);
    gap_set_scan_params(!bleAddressSet(), 0x0030, 0x0030, 0);
    gap_start_scan();
    return bleAddressSet();
}
