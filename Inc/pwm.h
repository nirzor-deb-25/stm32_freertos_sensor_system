#ifndef PWM_H
#define PWM_H

#include <stdbool.h>
#include <stdint.h>

/* Initialize green LD4 on PD12 at 1 kHz, initially off. */
void pwm_init(void);

/*
 * Set duty cycle from 0 to 100 percent.
 * Reject values greater than 100.
 *
 * After initialization, only the LED task may call this.
 */
bool pwm_set_percent(uint32_t percent);

#endif