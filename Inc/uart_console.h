#ifndef UART_CONSOLE_H
#define UART_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"

typedef struct
{
    uint32_t received_bytes;
    uint32_t dropped_bytes;
    uint32_t receive_errors;
    uint32_t transmit_timeouts;
} uart_diagnostics_t;

extern volatile uart_diagnostics_t uart_diagnostics;

/* Call once from the UART task after the scheduler starts. */
void uart_console_init(void);

/* Wait for a receive event or timeout. */
bool uart_console_wait(TickType_t timeout);

/* Read one buffered byte without blocking. Single consumer only. */
bool uart_console_read(uint8_t *value);

/* Transmit text. Only the UART task should call this function. */
bool uart_console_write(const char *text);

#endif