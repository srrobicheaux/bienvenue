// ultrasonic.c — 40 kHz FMCW with quadrature sampling on RP2040
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
#include "chirp.pio.h"
#include "ultrasonic.h"

#define TX_PIN_POS  0
#define TX_PIN_NEG  3
#define RX_ADC_GPIO 26

int32_t adc_buffer_i[16384];
int32_t adc_buffer_q[16384];

static PIO   pio;
static uint  sm;
static uint  dma_tx;
static volatile uint32_t sample_counter = 0;

void __isr pio_irq_handler(void) {
    pio_interrupt_clear(pio0, 0);

    uint16_t raw = adc_read();
    int16_t  val = (int16_t)(raw - 2048);  // centre at 0

    bool is_q        = (sample_counter & 1);
    bool is_negative = (sample_counter & 2);
    if (is_negative) val = -val;

    int32_t *buf = is_q ? adc_buffer_q : adc_buffer_i;
    buf[sample_counter >> 5] += val;   // 32 cycles per complex sample

    sample_counter++;
}

void core1_entry(void) {
    irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq_handler);
    irq_set_enabled(PIO0_IRQ_0, true);
    while (1) __wfi();
}

bool radar_init(void) {
    set_sys_clock_khz(256000, true);

    multicore_launch_core1(core1_entry);

    adc_gpio_init(RX_ADC_GPIO);
    adc_init();
    adc_select_input(0);
    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(0);

    pio = pio0;
    uint offset = pio_add_program(pio, &chirp_program);
    sm = pio_claim_unused_sm(pio, true);

    chirp_program_init(pio, sm, offset, TX_PIN_POS, TX_PIN_NEG);

    pio_set_irq0_source_enabled(pio, pis_interrupt0, true);

    dma_tx = dma_claim_unused_channel(true);

    printf("40 kHz FMCW radar initialized\n");
    return true;
}

void radar_run_cycle(void) {
    sample_counter = 0;
    memset(adc_buffer_i, 0, sizeof(adc_buffer_i));
    memset(adc_buffer_q, 0, sizeof(adc_buffer_q));

    dma_channel_config cfg = dma_channel_get_default_config(dma_tx);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_dreq(&cfg, pio_get_dreq(pio, sm, true));
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);

    dma_channel_configure(dma_tx, &cfg,
        &pio->txf[sm],
        chirp_buffer,
        CHIRP_LENGTH,
        true);

    dma_channel_wait_for_finish_blocking(dma_tx);
    busy_wait_us(15000);  // wait for echoes (up to ~5 m)
}
