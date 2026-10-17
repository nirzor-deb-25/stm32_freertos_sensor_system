#ifndef UART_TASK_H
#define UART_TASK_H

/*
 * argument must be the sensor-sample QueueHandle_t.
 * This task exclusively owns console transmission.
 */
void uart_task(void *argument);

#endif