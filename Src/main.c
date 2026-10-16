#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "stm32_regs.h"
#include "rtos_hooks.h"

#define GREEN_LED_MASK (1UL << 12)
#define BLUE_LED_MASK  (1UL << 15)
#define ALL_LED_MASK   (0xFUL << 12)

/* GPIOD registers used by the onboard LEDs. */
#define LED_MODER      REG32(0x40020C00UL)
#define LED_OTYPER     REG32(0x40020C04UL)
#define LED_OSPEEDR    REG32(0x40020C08UL)
#define LED_PUPDR      REG32(0x40020C0CUL)
#define LED_BSRR       REG32(0x40020C18UL)

/* Diagnostic values available through GDB/SWD. */
volatile uint32_t rtos_stage = 0U;
volatile uint32_t green_toggle_count = 0U;
volatile uint32_t blue_toggle_count = 0U;

static void leds_init(void)
{
    RCC_AHB1ENR |= (1UL << 3);
    (void)RCC_AHB1ENR;

    /* All LEDs initially off. */
    LED_BSRR = ALL_LED_MASK << 16;

    LED_OTYPER &= ~ALL_LED_MASK;
    LED_OSPEEDR &= ~(0xFFUL << 24);
    LED_PUPDR &= ~(0xFFUL << 24);

    /* PD12-PD15: general-purpose outputs. */
    LED_MODER =
        (LED_MODER & ~(0xFFUL << 24)) |
        (0x55UL << 24);
}

static void green_task(void *argument)
{
    (void)argument;

    bool on = false;
    TickType_t previous_wake = xTaskGetTickCount();

    rtos_stage = 3U;

    for (;;)
    {
        on = !on;

        LED_BSRR = on
            ? GREEN_LED_MASK
            : GREEN_LED_MASK << 16;

        green_toggle_count++;

        (void)xTaskDelayUntil(
            &previous_wake,
            pdMS_TO_TICKS(500U)
        );
    }
}

static void blue_task(void *argument)
{
    (void)argument;

    bool on = false;
    TickType_t previous_wake = xTaskGetTickCount();

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

    BaseType_t result = xTaskCreate(
        green_task,
        "Green",
        256U,
        0,
        1U,
        0
    );

    configASSERT(result == pdPASS);

    result = xTaskCreate(
        blue_task,
        "Blue",
        256U,
        0,
        1U,
        0
    );

    configASSERT(result == pdPASS);

    rtos_stage = 2U;

    vTaskStartScheduler();

    /* A successfully started scheduler does not return here. */
    rtos_panic(
        RTOS_FAILURE_SCHEDULER,
        __FILE__,
        __LINE__
    );
}