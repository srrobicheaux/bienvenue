#ifndef FLASH_H
#define FLASH_H
#include <stdint.h>

typedef struct {
    uint16_t version;    // Useful if you change the struct layout later
    char ssid[32];
    char password[64];
    char bleTarget[32];
    char device_id[32];
} DeviceSettings;

// Point to the location in memory where flash is mapped
extern const DeviceSettings *flash_data;

bool load_settings(DeviceSettings *dest, const char* current_device_id);
void save_settings(DeviceSettings *src);

//Misc hardware related items
bool get_bootsel_button();
float ChipTemp();

#endif // FLASH_H