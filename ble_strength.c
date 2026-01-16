#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"
#include "ad_parser.h"
#include "flash.h"
#include "regression.h"

#include <stdio.h>
#define DEBUG

#define OUT_OF_GARAGE_DB -75
#define MAX_AGE_UNSEEN 5000000 // 5 seconds in microseconds
// #define TRIGGER_DB .3
//  #define TRIGGER_DB .2
// #define MIN_TREND 1000000

// #define MIN_TREND 100

const void (*notify)(char *json, size_t size);

bd_addr_t bleAddress;
char *bleName; //Points to settings->bleTarget

static btstack_packet_callback_registration_t hci_event_callback_registration;

bool DiscoveryMAC(uint8_t *packet, bd_addr_t address)
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
// list names of BLE devices
#ifdef DEBUG
            char name[len];
            memcpy(name, data, len);
            name[len] = '\0';
            printf("Looking for %s, found %s - %s\n",bleName, bd_addr_to_str_with_delimiter(address, ':'), name);
#endif

            if (bleName[0] != '\0' && memcmp(data, bleName, len) == 0)
            {
                bd_addr_copy(bleAddress, address);
                DeviceSettings *update=load_settings();
                strcpy(settings.bleTarget, bd_addr_to_str(address));
                printf("Replacing BLE Target %s with MAC - %s\n",bleName,update->bleTarget);
                save_settings();
                return true;
            }
        }
    }
    return false;
}

// --- BLE Callback ---
void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    char payload[128];
    static bool bleAddressSet = false;

    if (packet_type != HCI_EVENT_PACKET)
        return;

    if (hci_event_packet_get_type(packet) == GAP_EVENT_ADVERTISING_REPORT)
    {
        bd_addr_t address;
        gap_event_advertising_report_get_address(packet, address);
        int8_t rssi = gap_event_advertising_report_get_rssi(packet);

//        printf("%s\n",bd_addr_to_str(bleAddress));
// CASE 1: No MAC yet, searching by Name
        if (!bleAddressSet){
            bleAddressSet=sscanf_bd_addr(bleName,bleAddress);
            if (!bleAddressSet) {
                bleAddressSet=DiscoveryMAC(packet, address);
            }
        }
        if (bd_addr_cmp(address, bleAddress) == 0)
        {
            float trend = get_gaussian_trend(rssi);
            snprintf(payload, sizeof(payload),
                     "data: {\"rssi\":%d,\"time\":%u,\"trend\":\"%d\"}\r\r\n",
                     rssi, get_absolute_time(), trend);
//            printf("BLE:%s\n", payload);
            notify(payload, strlen(payload));
        }
    }
}

bool BLE_Init(char *Target_Name, void (*notifer)(char *json, size_t size))
{
    // Initialize BTstack (Standard boiler plate)
    bleName=Target_Name;
    notify = notifer;
    bool bleAddressSet = sscanf_bd_addr(bleName, bleAddress);
    #ifdef DEBUG
        printf("Bluetooth scanning started for %s.\n", (bleAddressSet) ? bd_addr_to_str_with_delimiter(bleAddress, ':') : bleName);
    #endif

    l2cap_init();
    le_device_db_init();
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    hci_power_control(HCI_POWER_ON);
//    gap_set_local_name(CYW43_HOST_NAME);
    gap_set_scan_duplicate_filter(0);
    gap_set_scan_params(!bleAddressSet, 0x0030, 0x0030, 0);
    gap_start_scan();
    return bleAddressSet;
}
