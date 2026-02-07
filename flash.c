#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include "pico/stdio.h"
#include "pico/unique_id.h"
#include "pico/cyw43_arch.h"
#include "malloc.h"
#include "flash.h"
#include "regression.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

#define RESET_TIME 15000000 // 150000 // 15s

// Pick a unique hex code to identify YOUR data
#define SETTINGS_VERSION 0x0001
#define FLASH_TARGET_OFFSET (1536 * 1024)

// Point to the location in memory where flash is mapped
const DeviceSettings *flash_data = (const DeviceSettings *)(XIP_BASE + FLASH_TARGET_OFFSET);
DeviceSettings settings;

DeviceSettings *load_settings()
{
        char id_str[32];
    pico_get_unique_board_id_string(id_str, sizeof(id_str));
    printf("%s ID:\t%s\n", CYW43_HOST_NAME, id_str);
    memcpy(&settings, flash_data, sizeof(DeviceSettings));

    // Check if the ID number matches
    if (strcmp(settings.device_id, id_str) != 0)
    {
        // DATA IS INVALID: Set defaults
        printf("No valid settings found. Initializing defaults...\n");
        settings.version = SETTINGS_VERSION;
        strncpy(settings.ssid, "", 32);
        strncpy(settings.password, "", 64);
        strncpy(settings.bleName, "", 32);
        settings.position = 0;
        strncpy(settings.device_id, id_str, 32);
        memchr(settings.bleAddress,'\0',sizeof(bd_addr_t));

        printf("Settings:\tInitializing %s\n",bd_addr_to_str(settings.bleAddress));
    }
    else
    {
        printf("Settings:\tLoaded\n");
        // DATA IS VALID: Copied from flash to RAM
    }
    return &settings;
}
void saveorreset_settings(bool reset)
{
    // Buffer must be a multiple of FLASH_PAGE_SIZE (256)
    uint8_t buffer[FLASH_PAGE_SIZE];

    if (!reset)
    {
        printf("Saving %s\n", settings.ssid);
        memcpy(buffer, &settings, sizeof(DeviceSettings));
        printf("Settings:\tSaved\n");
    }
    else
    {
        printf("Settings:\tReset\n");
    }

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    if (!reset) {
        flash_range_program(FLASH_TARGET_OFFSET, buffer, FLASH_PAGE_SIZE);
    }
    restore_interrupts(ints);
}
void save_settings(){
    //only expost reset to flash.c
    saveorreset_settings(false);
}
void format()
{
    int sector = 1;
    while (PICO_FLASH_SIZE_BYTES < FLASH_TARGET_OFFSET + (FLASH_SECTOR_SIZE * sector))
    {
        flash_range_erase(FLASH_TARGET_OFFSET + (FLASH_SECTOR_SIZE * sector), FLASH_SECTOR_SIZE);
        printf("Formated Sector:%d\n", sector);
        stdio_flush();
        sleep_ms(10);
        sector++;
    }
}

#define PAGES__PER_SAVE 8 //Valid options [1,2,4,8,16 =] 16 pages per sector and must be a multiple of sector (4096 bytes) 
void save_history(uint8_t *buffer)
{
    uint32_t ints = save_and_disable_interrupts();
    if ((settings.position % (16/PAGES__PER_SAVE)) == 0)
    {
        int sector = (settings.position / (16/PAGES__PER_SAVE));
        // skip first sector to reserve it for settings
        flash_range_erase(FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE + (PAGES__PER_SAVE * FLASH_PAGE_SIZE * settings.position), FLASH_SECTOR_SIZE);
    }
    flash_range_program(FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE + (PAGES__PER_SAVE * FLASH_PAGE_SIZE * settings.position), buffer, FLASH_PAGE_SIZE*PAGES__PER_SAVE);
    restore_interrupts(ints);
    printf("Saved:%d bytes @ position:%d\n", sizeof(ble_event_t), settings.position);
    settings.position = settings.position + 1;
    //flash drive is full loop around to write over earlier data
    if (settings.position > 637 * (16/PAGES__PER_SAVE))
    {
         settings.position =0;
    }
}
const uint8_t *read_history(int position)
{
    return (const uint8_t *)XIP_BASE + FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE + (8 *FLASH_PAGE_SIZE * position);
}
/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// This example blinks the Pico LED when the BOOTSEL button is pressed.
//
// Picoboard has a button attached to the flash CS pin, which the bootrom
// checks, and jumps straight to the USB bootcode if the button is pressed
// (pulling flash CS low). We can check this pin in by jumping to some code in
// SRAM (so that the XIP interface is not required), floating the flash CS
// pin, and observing whether it is pulled low.
//
// This doesn't work if others are trying to access flash at the same time,
// e.g. XIP streamer, or the other core.

bool __no_inline_not_in_flash_func(get_bootsel_button)()
{
    const uint CS_PIN_INDEX = 1;

    // Must disable interrupts, as interrupt handlers may be in flash, and we
    // are about to temporarily disable flash access!
    uint32_t flags = save_and_disable_interrupts();

    // Set chip select to Hi-Z
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Note we can't call into any sleep functions in flash right now
    for (volatile int i = 0; i < 1000; ++i)
        ;

    // The HI GPIO registers in SIO can observe and control the 6 QSPI pins.
    // Note the button pulls the pin *low* when pressed.
#if PICO_RP2040
#define CS_BIT (1u << 1)
#else
#define CS_BIT SIO_GPIO_HI_IN_QSPI_CSN_BITS
#endif
    bool button_state = !(sio_hw->gpio_hi_in & CS_BIT);

    // Need to restore the state of chip select, else we are going to have a
    // bad time when we return to code in flash!
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);

    return button_state;
}

float ChipTemp()
{
    // 1. Initialize the ADC hardware (if not done already in main)
    // adc_init();

    // 2. CRITICAL: Enable the internal temperature sensor
    adc_set_temp_sensor_enabled(true);

    // 3. Select ADC input 4 (Internal Temp Sensor for Pico 1 & Pico 2)
    adc_select_input(ADC_TEMPERATURE_CHANNEL_NUM);

    // 4. Read raw value
    uint16_t raw_result = adc_read();

    // 5. Convert to voltage
    const float conversion_factor = 3.3f / (1 << 12);
    float voltage = raw_result * conversion_factor;

    // 6. Calculate Temperature (Formula is valid for RP2040 & RP2350)
    float temperature_celsius = 27.0f - (voltage - 0.706f) / 0.001721f;

    return 9 / 5 * temperature_celsius + 32;
}

// These are defined by the linker script
extern char __flash_binary_start;
extern char __flash_binary_end;

void get_memory_stats(uint32_t *ram_used, uint32_t *ram_total, uint32_t *flash_used, uint32_t *flash_total)
{
    // --- RAM (HEAP) USAGE ---
    // mallinfo() is a standard C library function to report heap status
    struct mallinfo m = mallinfo();

    // uordblks = Total allocated space (used)
    // fordblks = Total free space
    *ram_used = m.uordblks;
    *ram_total = m.uordblks + m.fordblks; // Total Heap available to your app

    // --- FLASH USAGE ---
    // Calculate the size of the binary by subtracting the end address from the start
    *flash_used = (uint32_t)(&__flash_binary_end - &__flash_binary_start);

    // The Pico 2 W typically has 4MB of Flash (check your specific board specs)
    // You can also use PICO_FLASH_SIZE_BYTES if defined in board header
    *flash_total = 4 * 1024 * 1024;
}

void send_status_event(void (*notifer)(char *json, size_t size))
{
    uint32_t ram_used, ram_total, flash_used, flash_total;
    get_memory_stats(&ram_used, &ram_total, &flash_used, &flash_total);

    float ram_pct = (float)ram_used / ram_total * 100.0f;
    float flash_pct = (float)flash_used / flash_total * 100.0f;

    static char buffer[256];
    snprintf(buffer, sizeof(buffer),
             "data: {\"type\":\"sys\", \"ram_used\":%lu, \"ram_total\":%lu, \"ram_pct\":%.1f, \"flash_pct\":%.1f,\"temp_f\":%.2f}\r\r\n",
             // Correct JSON format: uses ':' for value and ends with single '}'
             ram_used, ram_total, ram_pct, flash_pct, ChipTemp());
    notifer(buffer, strlen(buffer));
    //        printf("Memory:%s\n", buffer);
}

void reset()
{
    watchdog_enable(10, true); // true = reset on timeout
    while (1)
        ;
}
void ButtonPress()
{
    watchdog_disable();
    absolute_time_t LongPress =   get_absolute_time() + 10000000; //~10seconds

    while (get_bootsel_button())
        ;
    // if more than ~10s held, reset settings
    if (time_reached(LongPress))
    {
        // blank id to indicate uninitialized

        printf("Button held long enough, resetting settings.\n");
        saveorreset_settings(true);// Reset Flash
    }
    else
    {
        printf("Short Button press, rebooting only.\n");
    }
    reset();
}

void touchBase()
{
    static bool initial = true;
    static bool blink = false;
    blink = !blink;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, blink);

    if (initial)
    {
        watchdog_enable(RESET_TIME, true);
        initial = false;
    }

    watchdog_update();

    // Button Pressed: either reset settings or just reboot
    if (get_bootsel_button())
    {
        printf("Button Pressed! System will reboot. Hold for 15 seconds for flash reset.\n");
        ButtonPress();
        sleep_ms(10);
    }
}

void setup_pins(int pin)
{
    // 1. Initialize the RP2040 pins
    gpio_init(pin);

    // 2. Set direction to OUTPUT
    gpio_set_dir(pin, GPIO_OUT);
}

bool pin(int pin, bool toggle)
{
    // 3. Use standard gpio_put
    if (toggle)
    {
        if (gpio_get_dir(pin) == GPIO_IN)
        {
            setup_pins(pin);
        }
        gpio_put(pin, !gpio_get(pin)); // Turn ON
    }
    return gpio_get(pin);
}