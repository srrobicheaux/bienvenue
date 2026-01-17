#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/clocks.h" // <--- ADDED THIS TO FIX ERRORS
#include "ultrasonic.h"
#include "chirp.pio.h"

#define COMPLEX_SAMPLE_RATE_HZ 100000.0f

#define SPEED_OF_SOUND_MPS 343.0f
#define PICO_DEFAULT_LED_PIN 29
#define CHIRP_PIN 5  // Physical Pin 7 is GPIO 5

uint16_t adc_buffer_i[CHIRP_LENGTH];
uint16_t adc_buffer_q[CHIRP_LENGTH];

static int dma_chan_i;
static PIO pio_hw = pio0;
static uint sm = 0;

void internal_chirp_init(PIO pio, uint sm, uint offset, uint pin)
{
    pio_sm_config c = chirp_program_get_default_config(offset);

    // CRITICAL: This MUST match the 'set pins' in the PIO code
    sm_config_set_set_pins(&c, pin, 1);

    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    // Speed: 1MHz makes the [12] delays result in 40kHz
    float div = (float)clock_get_hz(clk_sys) / 1000000.0f;
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

bool radar_init()
{
    adc_init();
    // 1. Change GPIO from 26 to 27x
gpio_set_pulls(27, true, false); // Pull-up only
    adc_gpio_init(27);

    // 2. Select Input 1 (GPIO 27 is ADC1)
    adc_select_input(1);

    adc_fifo_setup(true, true, 1, false, false);
// 48,000,000 / 480 = 100,000 Hz (100kHz)
adc_set_clkdiv(479); 
// Then update your constant:

    uint offset = pio_add_program(pio_hw, &chirp_program);
    //    internal_chirp_init(pio_hw, sm, offset, 0);
    internal_chirp_init(pio_hw, sm, offset, CHIRP_PIN);

    dma_chan_i = dma_claim_unused_channel(true);
    dma_channel_config c_i = dma_channel_get_default_config(dma_chan_i);
    channel_config_set_transfer_data_size(&c_i, DMA_SIZE_16);
    channel_config_set_read_increment(&c_i, false);
    channel_config_set_write_increment(&c_i, true);

    // 3. Keep DREQ_ADC (This paces the DMA to the ADC's speed regardless of channel)
    channel_config_set_dreq(&c_i, DREQ_ADC);

    dma_channel_configure(dma_chan_i, &c_i, adc_buffer_i, &adc_hw->fifo, CHIRP_LENGTH, false);

    return true;
}
void radar_run_cycle()
{
    adc_fifo_drain();
    adc_run(false);

    dma_channel_abort(dma_chan_i);
    dma_channel_set_write_addr(dma_chan_i, adc_buffer_i, true);

    adc_run(true);                                    // 1. Listen first
    pio_sm_put_blocking(pio_hw, sm, 1);               // 2. Pulse 40kHz
    dma_channel_wait_for_finish_blocking(dma_chan_i); // 3. Wait
    adc_run(false);
}

/**
 * Process the ADC buffer to find the strongest echo.
 */
void process_one_beam(int len, Detection *det)
{
    // 1. DYNAMIC AUTO-CALIBRATION
    // We sample the first few values to find the DC center (Bias).
    // This adapts automatically whether Bias is 930 or 2048.
    int64_t sum_dc = 0;
    for (int i = 0; i < 32; i++)
    {
        sum_dc += adc_buffer_i[i];
    }
    int32_t local_center = (int32_t)(sum_dc / 32);

    // 2. NOISE FLOOR ESTIMATION
    // Measure variance in the early part of the buffer.
    int64_t noise_sum = 0;
    for (int i = 10; i < 50; i++)
    {
        int32_t sample = (int32_t)adc_buffer_i[i] - local_center;
        noise_sum += (int64_t)sample * sample;
    }
    int64_t noise_floor = noise_sum / 40;
    if (noise_floor < 10)
        noise_floor = 10;

    // 3. SEARCH FOR ECHO PEAK
    int64_t max_mag_sq = 0;
    int max_idx = -1;

    // skip_samples: Ignore "Main Bang" (transmitter ringing).
    // Increased to 220 because the 12V pulse rings longer.
    const int skip_samples = 50;

    for (int i = skip_samples; i < len - 10; i++)
    {
        int32_t sample = (int32_t)adc_buffer_i[i] - local_center;
        int64_t mag_sq = (int64_t)sample * sample;

        if (mag_sq > max_mag_sq)
        {
            max_mag_sq = mag_sq;
            max_idx = i;
        }
    }

    // 4. THRESHOLDING AND DISTANCE
    // Threshold set to 1500 to ignore small air turbulence but catch real walls.
    if (max_idx > skip_samples && max_mag_sq > 2500)
    { // Increased from 1500
        det->amplitude_sq = (uint32_t)max_mag_sq;

        // Math: (index / sample_rate) * speed_of_sound / 2 (for round trip)
        float t_sec = (float)max_idx / COMPLEX_SAMPLE_RATE_HZ;
        float dist_m = (t_sec * SPEED_OF_SOUND_MPS) / 2.0f;
        det->distance_mm = (uint16_t)(dist_m * 1000.0f);
    }
    else
    {
        det->distance_mm = 0;
        det->amplitude_sq = 0;
    }

    // Console Debug Output (every 10 samples for better visibility)
    static int debug_count = 0;
    if (++debug_count % 10 == 0)
    {
//        printf("Bias: %d | Peak: %d | Mag: %lld | Noise: %lld | Dist: %dmm\n",
//               local_center, max_idx, max_mag_sq, noise_floor, det->distance_mm);
    }
}
