float get_gaussian_trend(int8_t new_rssi);

// --- Tunable Parameters ---
#define HISTORY_SIZE 250//75   (I want to historize about 30s to review rssi before arrival.)    // 40 samples (approx 2-4 seconds of history)
//#define GAUSSIAN_SIGMA 10.0f  // "Width" of the bell curve. Lower = more reactive, Higher = smoother.
#define GAUSSIAN_SIGMA 700000.0f  // "Width" of the bell curve. Lower = more reactive, Higher = smoother.

typedef struct {
    int8_t rssi;
    uint32_t timestamp;
} rssi_sample_t;

typedef struct {
    rssi_sample_t history[HISTORY_SIZE];
    uint16_t size;
    float slope;
    absolute_time_t time;
} ble_event_t;

