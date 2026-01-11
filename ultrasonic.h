// ultrasonic.h
#pragma once
#include <stdbool.h>

extern int32_t adc_buffer_i[];
extern int32_t adc_buffer_q[];
extern const int16_t chirp_buffer[];
extern const int CHIRP_LENGTH;

bool radar_init(void);
void radar_run_cycle(void);
