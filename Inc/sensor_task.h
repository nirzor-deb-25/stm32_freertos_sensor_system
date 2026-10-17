#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

typedef struct
{
    uint32_t sequence;
    TickType_t timestamp;
    int32_t x_mg;
    int32_t y_mg;
    int32_t z_mg;
} sensor_sample_t;

typedef struct
{
    uint32_t id;
    uint32_t configured;
    uint32_t spi_status;
    uint32_t sample_count;
    uint32_t dropped_count;
    uint32_t error_count;
    uint32_t not_ready_count;
} sensor_diagnostics_t;

extern volatile sensor_diagnostics_t sensor_diagnostics;

/*
 * argument must be a QueueHandle_t for a queue whose
 * item size is sizeof(sensor_sample_t).
 *
 * This task exclusively owns the SPI sensor driver.
 */
void sensor_task(void *argument);

#endif