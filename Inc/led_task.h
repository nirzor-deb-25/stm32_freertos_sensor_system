#ifndef LED_TASK_H
#define LED_TASK_H

#include <stdint.h>

#define LED_QUEUE_LENGTH 8U

typedef enum
{
    LED_MODE_STEADY = 0,
    LED_MODE_BLINK,
    LED_MODE_BREATH
} led_mode_t;

typedef enum
{
    LED_COMMAND_BRIGHTNESS = 0,
    LED_COMMAND_MODE
} led_command_type_t;

typedef struct
{
    led_command_type_t type;
    uint32_t value;
} led_command_t;

typedef struct
{
    uint32_t brightness;
    uint32_t mode;
    uint32_t output_percent;
    uint32_t applied_commands;
    uint32_t rejected_commands;
} led_state_t;

/* argument must be the LED command QueueHandle_t. */
void led_task(void *argument);

/* Return a synchronized copy of the LED task's state. */
led_state_t led_get_state(void);

#endif