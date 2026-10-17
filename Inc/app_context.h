#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include "FreeRTOS.h"
#include "queue.h"

/* Shared queue handles passed to the UART task. */
typedef struct
{
    QueueHandle_t sample_queue;
    QueueHandle_t led_queue;
} app_context_t;

#endif