#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include <stdio.h>
#include <string.h>

// Assume these from your project
#define FLASH_SIZE (2 * 1024 * 1024)  // 2MB for Pico W
#define FLASH_SECTOR_SIZE 4096
#define FLASH_PAGE_SIZE 256

// History storage: last 64KB (16 sectors) for ring buffer
#define HISTORY_AREA_OFFSET (FLASH_SIZE - (64 * 1024))
#define HISTORY_AREA_SIZE (64 * 1024)
#define HISTORY_ENTRIES (HISTORY_AREA_SIZE / sizeof(UltrasonicHistory))  // e.g., ~1000+ entries

// Magic for valid entry
#define HISTORY_MAGIC 0x554C5452  // "ULTR"


// Global for current entry (RAM cache)
static UltrasonicHistory current_entry;

// Initialize history system (call after loading settings)
void history_init(DeviceSettings *settings) {
    settings->history_write_index = 0;
    settings->history_count = 0;
    // Optional: erase entire history area on first boot/format
    // flash_range_erase(HISTORY_AREA_OFFSET, HISTORY_AREA_SIZE);
}

// Get pointer to history entry at slot (no copy – direct flash map)
static const UltrasonicHistory *get_history_slot(uint16_t slot) {
    uint32_t addr = HISTORY_AREA_OFFSET + (slot * sizeof(UltrasonicHistory));
    return (const UltrasonicHistory *)(XIP_BASE + addr);
}

// Find latest valid entry (for display/startup)
const UltrasonicHistory *history_get_latest(const DeviceSettings *settings) {
    if (settings->history_count == 0) return NULL;

    uint16_t slot = (settings->history_write_index == 0) ?
        (HISTORY_ENTRIES - 1) :
        (settings->history_write_index - 1);

    const UltrasonicHistory *entry = get_history_slot(slot);
    if (entry->magic_number == HISTORY_MAGIC) {
        return entry;
    }
    return NULL;  // Corrupt – consider full erase
}

// Log new ultrasonic reading (quick, no full erase)
void history_log(DeviceSettings *settings, uint16_t distance_cm, int rssi /* add your data */) {
    current_entry.magic_number = HISTORY_MAGIC;
    current_entry.version = HISTORY_VERSION;
    current_entry.timestamp = to_us_since_boot(get_absolute_time()) / 1000000;  // seconds
    current_entry.distance_cm = distance_cm;
    current_entry.rssi = rssi;
    // ... add other fields ...

    uint16_t slot = settings->history_write_index;
    uint32_t flash_addr = HISTORY_AREA_OFFSET + (slot * sizeof(UltrasonicHistory));

    // Prepare page-aligned buffer
    uint8_t buffer[FLASH_PAGE_SIZE] = {0};
    memcpy(buffer, &current_entry, sizeof(UltrasonicHistory));

    uint32_t ints = save_and_disable_interrupts();
    // Erase only the sector containing this slot if first write to it after wrap
    if (settings->history_count >= HISTORY_ENTRIES) {
        // Full – erase next sector when wrapping (simple wear leveling)
        uint32_t sector_offset = (flash_addr - HISTORY_AREA_OFFSET) & ~(FLASH_SECTOR_SIZE - 1);
        flash_range_erase(HISTORY_AREA_OFFSET + sector_offset, FLASH_SECTOR_SIZE);
    }

    flash_range_program(flash_addr, buffer, FLASH_PAGE_SIZE);
    restore_interrupts(ints);

    // Update index
    settings->history_write_index = (settings->history_write_index + 1) % HISTORY_ENTRIES;
    if (settings->history_count < HISTORY_ENTRIES) {
        settings->history_count++;
    }

    // Save updated index/count to settings flash (your existing save_settings)
    save_settings(settings);
}

// Iterate all valid history (for web display)
void history_foreach(void (*callback)(const UltrasonicHistory *entry, uint16_t index)) {
    // Assume settings loaded
    extern DeviceSettings settings;

    uint16_t count = settings.history_count;
    if (count == 0) return;

    uint16_t start = (settings.history_write_index == 0) ?
        (HISTORY_ENTRIES - count) :
        (settings.history_write_index - count);

    for (uint16_t i = 0; i < count; i++) {
        uint16_t slot = (start + i) % HISTORY_ENTRIES;
        const UltrasonicHistory *entry = get_history_slot(slot);
        if (entry->magic_number == HISTORY_MAGIC) {
            callback(entry, i);
        }
    }
}