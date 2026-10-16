#ifndef TIMING_H
#define TIMING_H

#include <stdbool.h>
#include <stdint.h>

void timing_init(void);

uint32_t timing_cycles(void);

bool timing_has_elapsed(uint32_t start_cycles, uint32_t timeout_ms);

void timing_delay_ms(uint32_t milliseconds);

#endif