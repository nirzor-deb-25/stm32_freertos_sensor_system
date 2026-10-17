#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "led_task.h"
#include "pwm.h"

#define LED_UPDATE_MS 10U

static led_state_t published_state = {0};

led_state_t led_get_state(void)
{
    led_state_t snapshot;

    taskENTER_CRITICAL();
    snapshot = published_state;
    taskEXIT_CRITICAL();

    return snapshot;
}

static uint32_t calculate_output(
    uint32_t brightness,
    led_mode_t mode,
    TickType_t elapsed)
{
    if (mode == LED_MODE_STEADY)
    {
        return brightness;
    }

    if (mode == LED_MODE_BLINK)
    {
        TickType_t phase = elapsed % pdMS_TO_TICKS(1000U);

        return (phase < pdMS_TO_TICKS(500U))
            ? brightness
            : 0U;
    }

    /*
     * Breathing: one second rising, one second falling.
     * Brightness sets the maximum duty cycle.
     */
    TickType_t half_cycle = pdMS_TO_TICKS(1000U);
    TickType_t phase = elapsed % pdMS_TO_TICKS(2000U);

    uint32_t ramp;

    if (phase <= half_cycle)
    {
        ramp = (uint32_t)phase;
    }
    else
    {
        ramp = (uint32_t)(pdMS_TO_TICKS(2000U) - phase);
    }

    return (brightness * ramp) / (uint32_t)half_cycle;
}

void led_task(void *argument)
{
    QueueHandle_t command_queue = (QueueHandle_t)argument;

    configASSERT(command_queue != 0);
    configASSERT(pdMS_TO_TICKS(LED_UPDATE_MS) > 0U);
    configASSERT(pdMS_TO_TICKS(1000U) > 0U);

    pwm_init();

    uint32_t brightness = 0U;
    led_mode_t mode = LED_MODE_STEADY;

    uint32_t applied = 0U;
    uint32_t rejected = 0U;

    TickType_t mode_start = xTaskGetTickCount();
    TickType_t previous_wake = mode_start;

    for (;;)
    {
        /*
         * Process a bounded number of commands so that
         * continuous input cannot prevent PWM updates.
         */
        for (uint32_t count = 0U; count < LED_QUEUE_LENGTH; count++)
        {
            led_command_t command;

            if (xQueueReceive(
                    command_queue,
                    &command,
                    0U) != pdPASS)
            {
                break;
            }

            bool accepted = false;

            if (command.type == LED_COMMAND_BRIGHTNESS)
            {
                if (command.value <= 100U)
                {
                    brightness = command.value;
                    accepted = true;
                }
            }
            else if (command.type == LED_COMMAND_MODE)
            {
                if (command.value <= (uint32_t)LED_MODE_BREATH)
                {
                    mode = (led_mode_t)command.value;
                    mode_start = xTaskGetTickCount();
                    accepted = true;
                }
            }

            if (accepted)
            {
                applied++;
            }
            else
            {
                rejected++;
            }
        }

        TickType_t now = xTaskGetTickCount();

        uint32_t output = calculate_output(
            brightness,
            mode,
            (TickType_t)(now - mode_start)
        );

        bool result = pwm_set_percent(output);
        configASSERT(result);

        /*
         * Publish all fields together.
         * Readers use the same critical-section protection.
         */
        taskENTER_CRITICAL();

        published_state.brightness = brightness;
        published_state.mode = (uint32_t)mode;
        published_state.output_percent = output;
        published_state.applied_commands = applied;
        published_state.rejected_commands = rejected;

        taskEXIT_CRITICAL();

        (void)xTaskDelayUntil(
            &previous_wake,
            pdMS_TO_TICKS(LED_UPDATE_MS)
        );
    }
}