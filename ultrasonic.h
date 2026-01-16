#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t distance_mm;
    uint32_t amplitude_sq;
} Detection;


#define CHIRP_LENGTH 1024  // Ensure this matches your math

// Mark as extern so main.c can see them
extern uint16_t adc_buffer_i[CHIRP_LENGTH];
extern uint16_t adc_buffer_q[CHIRP_LENGTH];

bool radar_init(void);
void radar_run_cycle(void);
void process_one_beam(int len, Detection *det);

#endif