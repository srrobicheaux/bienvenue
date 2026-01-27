#ifndef FLASH_H
#define FLASH_H
#include <stdint.h>
#include <stddef.h>
#include "pico/time.h"
#include "btstack.h"

typedef struct {
    uint16_t version;    // Useful if you change the struct layout later
    char ssid[32];
    char password[64];
    char bleName[32];
    bd_addr_t bleAddress;
    char device_id[32];
    uint32_t position;
} DeviceSettings;

// Point to the location in memory where flash is mapped
extern DeviceSettings settings;

DeviceSettings *load_settings();
void save_settings();

//Misc hardware related items
bool get_bootsel_button();
void touchBase();
void reset();
bool pin(int pin, bool toggle);
void send_status_event(void (*notifer)(char *json, size_t size));
void save_history(uint8_t *buffer);
const uint8_t * read_history(int position);
void format();


typedef struct {
    uint16_t version;    // Useful if you change the struct layout later
    uint32_t distance;
    absolute_time_t event_ts;
    uint32_t peak;
    uint64_t magnitude;
    uint64_t noise;
} Sampling;


#endif // FLASH_H