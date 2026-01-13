#ifndef FLASH_H
#define FLASH_H
#include <stdint.h>

typedef struct {
    uint16_t version;    // Useful if you change the struct layout later
    char ssid[32];
    char password[64];
    char bleTarget[32];
    char device_id[32];
    bool initialized;
} DeviceSettings;

// Point to the location in memory where flash is mapped
extern DeviceSettings settings;

DeviceSettings *load_settings();
void save_settings();

//Misc hardware related items
bool get_bootsel_button();
float ChipTemp();

#endif // FLASH_H