#include "FreeRTOS.h"
#include "task.h"

#include "rtos_hooks.h"
#include "stm32_regs.h"

volatile uint32_t rtos_failure_code = 0U;
volatile uint32_t rtos_failure_line = 0U;

const char * volatile rtos_failure_file = 0;
const char * volatile rtos_failure_task = 0;

__attribute__((noreturn))
void rtos_panic(
    uint32_t code,
    const char *file,
    uint32_t line)
{
    __asm volatile ("cpsid i" ::: "memory");

    rtos_failure_code = code;
    rtos_failure_line = line;
    rtos_failure_file = file;

    /* Enable GPIOD. */
    RCC_AHB1ENR |= (1UL << 3);
    (void)RCC_AHB1ENR;

    /*
     * PD12: green, PD13: orange,
     * PD14: red, PD15: blue.
     *
     * Set red on and the other LEDs off.
     */
    REG32(0x40020C18UL) =
        (1UL << 14) |
        ((1UL << 12) | (1UL << 13) | (1UL << 15)) << 16;

    /* Configure PD12-PD15 as push-pull outputs. */
    REG32(0x40020C04UL) &= ~(0xFUL << 12);

    /* Low speed, no pull-up or pull-down. */
    REG32(0x40020C08UL) &= ~(0xFFUL << 24);
    REG32(0x40020C0CUL) &= ~(0xFFUL << 24);

    REG32(0x40020C00UL) =
        (REG32(0x40020C00UL) & ~(0xFFUL << 24)) |
        (0x55UL << 24);

    __asm volatile ("dsb" ::: "memory");

    /* Stop here so GDB can inspect the diagnostic variables. */
    for (;;)
    {
        __asm volatile ("nop");
    }
}

void rtos_assert_failed(const char *file, uint32_t line)
{
    rtos_panic(RTOS_FAILURE_ASSERT, file, line);
}

void vApplicationMallocFailedHook(void)
{
    rtos_panic(RTOS_FAILURE_MALLOC, __FILE__, __LINE__);
}

void vApplicationStackOverflowHook(
    TaskHandle_t task,
    char *task_name)
{
    (void)task;

    /* Record the pointer without dereferencing it here. */
    rtos_failure_task = task_name;

    rtos_panic(RTOS_FAILURE_STACK, __FILE__, __LINE__);
}