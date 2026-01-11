#include <math.h>
#include "pico/stdio.h"
#include "pico/time.h"

// --- Tunable Parameters ---
#define HISTORY_SIZE 40       // 40 samples (approx 2-4 seconds of history)
//#define GAUSSIAN_SIGMA 10.0f  // "Width" of the bell curve. Lower = more reactive, Higher = smoother.
#define GAUSSIAN_SIGMA 20.0f  // "Width" of the bell curve. Lower = more reactive, Higher = smoother.

typedef struct {
    int8_t rssi;
    uint32_t timestamp;
} rssi_sample_t;

// Returns trend as slope (dBm per second)
// Positive = Approaching, Negative = Leaving
float get_gaussian_trend(int8_t new_rssi) {
    static rssi_sample_t history[HISTORY_SIZE];
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
    if (count > 0 && now == history[(head - 1 + HISTORY_SIZE) % HISTORY_SIZE].timestamp) {
        return 0.0f; // Prevent divide by zero in time calc
    }

    history[head].rssi = new_rssi;
    history[head].timestamp = now;
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
        float y = (float)history[idx].rssi;
        
        // x = Time delta in Seconds (negative because it's in the past)
        // Using real time handles missing packets/jitter gracefully!
        float x = (float)(history[idx].timestamp - now) / 1000.0f;
        
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

    return slope; 
}