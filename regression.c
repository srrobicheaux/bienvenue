#include <math.h>
#include "pico/stdio.h"
#include "pico/time.h"
#include "regression.h"
#include <string.h>
#include "flash.h"
#include <stdio.h>

// Returns trend as slope (dBm per second)
// Positive = Approaching, Negative = Leaving
float get_gaussian_trend(int8_t new_rssi) {
    static float last_slope =-99;
    static uint8_t save_buffer[256*8];
    static ble_event_t *ble = (ble_event_t *) save_buffer; 
    static float weights[HISTORY_SIZE];
    static bool weights_init = false;
    static uint8_t head = 0;
    static uint8_t count = 0;
    
    // 1. One-time setup: Pre-compute Gaussian weights
    // We want the newest index to have the highest weight
    if (!weights_init) {
        for (int i = 0; i < HISTORY_SIZE; i++) {
            // x represents "how old" the sample is (0 = newest)
            float x = (float)i;
            // Standard Gaussian function: e^(-x^2 / 2sigma^2)
            weights[i] = expf(-(x * x) / (2.0f * GAUSSIAN_SIGMA * GAUSSIAN_SIGMA));
        }
        weights_init = true;
    }

    // 2. Add new sample to Circular Buffer
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // If packet is wildly old or duplicate, ignore (optional safety)
    if (count > 0 && now == ble->history[(head - 1 + HISTORY_SIZE) % HISTORY_SIZE].timestamp) {
        return 0.0f; // Prevent divide by zero in time calc
    }

    ble->history[head].rssi = new_rssi;
    ble->history[head].timestamp = now;
    head = (head + 1) % HISTORY_SIZE;
    if (count < HISTORY_SIZE) count++;

    // Need at least ~5 points to form a valid trend
    if (count < 5) return 0.0f;

    // 3. Gaussian Weighted Linear Regression
    // We want to solve for slope 'm' in: y = mx + c
    // But weighted by w_i
    
    float sum_w = 0.0f;
    float sum_wx = 0.0f;
    float sum_wy = 0.0f;
    float sum_wxy = 0.0f;
    float sum_wx2 = 0.0f;

    // We iterate backwards from newest to oldest
    for (int i = 0; i < count; i++) {
        // Get index in circular buffer (starting at newest)
        int idx = (head - 1 - i + HISTORY_SIZE) % HISTORY_SIZE;
        
        // y = RSSI
        float y = (float)ble->history[idx].rssi;
        
        // x = Time delta in Seconds (negative because it's in the past)
        // Using real time handles missing packets/jitter gracefully!
        float x = (float)(ble->history[idx].timestamp - now) / 1000.0f;
        
        // w = Gaussian Weight based on 'i' (staleness rank)
        float w = weights[i]; 

        sum_w   += w;
        sum_wx  += w * x;
        sum_wy  += w * y;
        sum_wxy += w * x * y;
        sum_wx2 += w * x * x;
    }

    // Weighted Determinant
    float denom = (sum_w * sum_wx2) - (sum_wx * sum_wx);

    if (fabs(denom) < 0.0001f) return 0.0f; // Avoid singularity

    // Slope m = (SumW * SumWXY - SumWX * SumWY) / Denom
    float slope = ((sum_w * sum_wxy) - (sum_wx * sum_wy)) / denom;

    static bool out_of_garage = true;
    if (false && (out_of_garage && new_rssi > -50 || !out_of_garage && new_rssi < -100))
    {
        printf("Saving:%f - %s\n",slope, (out_of_garage)? "Arriving" : "Leaving");
        out_of_garage = !out_of_garage;
        ble->slope = slope;
        ble->size = sizeof(ble_event_t);
        ble->time = get_absolute_time();
        // Buffer must be a multiple of FLASH_PAGE_SIZE (256)
        save_history(save_buffer);
        ble_event_t *fs_check;
        fs_check=(ble_event_t *)read_history(settings.position-1);

        printf("{\"position\":%2d},{\"slope\":%f},{\"size\":%3d}\n", settings.position-1, fs_check->slope, fs_check->size);
        printf("{history:[\n");
        for (size_t j = 0; j < 256; j++)
        {
        printf("\t{\"rssi\":%f},{\"micro_s\":%llu}\n", fs_check->history[j].rssi, fs_check->history[j].timestamp);
        }
        printf("]}\n");


        touchBase();
        save_settings(true);
    }
    return slope; 
}