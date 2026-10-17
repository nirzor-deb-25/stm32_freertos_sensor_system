#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "stm32_regs.h"
#include "rtos_hooks.h"
#include "sensor_task.h"
#include "uart_task.h"
#include "led_task.h"
#include "app_context.h"

#define BLUE_LED_MASK  (1UL << 15)
#define ALL_LED_MASK   (0xFUL << 12)

#define LED_MODER      REG32(0x40020C00UL)
#define LED_OTYPER     REG32(0x40020C04UL)
#define LED_OSPEEDR    REG32(0x40020C08UL)
#define LED_PUPDR      REG32(0x40020C0CUL)
#define LED_BSRR       REG32(0x40020C18UL)

#define SAMPLE_QUEUE_LENGTH 8U

volatile uint32_t rtos_stage = 0U;
volatile uint32_t blue_toggle_count = 0U;

/* Static storage keeps this valid for the lifetime of all tasks. */
static app_context_t app_context;

static void leds_init(void)
{
    RCC_AHB1ENR |= (1UL << 3);
    (void)RCC_AHB1ENR;

    LED_BSRR = ALL_LED_MASK << 16;

    LED_OTYPER &= ~ALL_LED_MASK;
    LED_OSPEEDR &= ~(0xFFUL << 24);
    LED_PUPDR &= ~(0xFFUL << 24);

    LED_MODER =
        (LED_MODER & ~(0xFFUL << 24)) |
        (0x55UL << 24);
}

static void blue_task(void *argument)
{
    (void)argument;

    bool on = false;
    TickType_t previous_wake = xTaskGetTickCount();

    rtos_stage = 3U;

    for (;;)
    {
        on = !on;

        LED_BSRR = on
            ? BLUE_LED_MASK
            : BLUE_LED_MASK << 16;

        blue_toggle_count++;

        (void)xTaskDelayUntil(
            &previous_wake,
            pdMS_TO_TICKS(125U)
        );
    }
}

int main(void)
{
    leds_init();

    rtos_stage = 1U;

    app_context.sample_queue = xQueueCreate(
        SAMPLE_QUEUE_LENGTH,
        sizeof(sensor_sample_t)
    );

    configASSERT(app_context.sample_queue != 0);

    app_context.led_queue = xQueueCreate(
        LED_QUEUE_LENGTH,
        sizeof(led_command_t)
    );

    configASSERT(app_context.led_queue != 0);

    vQueueAddToRegistry(
        app_context.sample_queue,
        "Samples"
    );

    vQueueAddToRegistry(
        app_context.led_queue,
        "LED commands"
    );

    BaseType_t result = xTaskCreate(
        blue_task,
        "Blue",
        256U,
        0,
        1U,
        0
    );

    configASSERT(result == pdPASS);

    result = xTaskCreate(
        led_task,
        "LED",
        384U,
        app_context.led_queue,
        1U,
        0
    );

    configASSERT(result == pdPASS);

    result = xTaskCreate(
        uart_task,
        "UART",
        768U,
        &app_context,
        1U,
        0
    );

    configASSERT(result == pdPASS);

    result = xTaskCreate(
        sensor_task,
        "Sensor",
        384U,
        app_context.sample_queue,
        2U,
        0
    );

    configASSERT(result == pdPASS);

    rtos_stage = 2U;

    vTaskStartScheduler();

    rtos_panic(
        RTOS_FAILURE_SCHEDULER,
        __FILE__,
        __LINE__
    );
}