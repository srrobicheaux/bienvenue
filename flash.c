#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/adc.h"
#include "pico/stdio.h"
#include "pico/unique_id.h"
#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "flash.h"

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
    char this_id[32];
    pico_get_unique_board_id_string(this_id, 32);

    memcpy(&settings, flash_data, sizeof(DeviceSettings));
    // Check if the ID number matches
    if (strcmp(settings.device_id, this_id) != 0)
    {
        // DATA IS INVALID: Set defaults
        printf("No valid settings found. Initializing defaults...\n");
        settings.version = SETTINGS_VERSION;
        strncpy(settings.ssid, "_", 32);
        strncpy(settings.password, "_", 64);
        strncpy(settings.bleTarget, "_", 32);
        settings.initialized = false;
        strncpy(settings.device_id, this_id, 32);
    }
    else
    {
        settings.initialized = true;
        // DATA IS VALID: Copied from flash to RAM
    }
    return &settings;
}

void save_settings()
{
    settings.version = SETTINGS_VERSION;
    // Buffer must be a multiple of FLASH_PAGE_SIZE (256)
    uint8_t buffer[FLASH_PAGE_SIZE];
    memset(buffer, 0, sizeof(buffer));

    if (&settings != NULL)
    {
        memcpy(buffer, &settings, sizeof(DeviceSettings));
        printf("Settings saved to flash.\n");
    }
    else
    {
        printf("Flash reset!\n");
    }

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, buffer, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
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
    // Select the ADC input for the temperature sensor (channel 4)
    adc_select_input(8);

    // Read the raw ADC value (12-bit conversion)
    uint16_t raw_result = adc_read();

    // Conversion factor for 3.3V reference, 12-bit resolution
    const float conversion_factor = 3.3f / (1 << 12);

    // Convert raw reading to voltage
    float voltage = raw_result * conversion_factor;

    // Apply the formula from the RP2040 datasheet (Chapter 4.9.5)
    // T = 27 - (ADC_Voltage - 0.706)/0.001721
    float temperature_celsius = 27.0f - (voltage - 0.706f) / 0.001721f;

    return temperature_celsius;
}

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

        save_settings();
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
    if (watchdog_get_time_remaining_us() > RESET_TIME)
    {
        watchdog_enable(RESET_TIME, true);
    }

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