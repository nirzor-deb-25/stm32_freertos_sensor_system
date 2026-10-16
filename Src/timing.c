#include "timing.h"
#include "stm32_regs.h"

#define CYCLES_PER_MS  16000UL
#define MAX_TIMEOUT_MS 60000U

void timing_init(void)
{
    /* Enable access to the Cortex-M4 trace facilities. */
    CORE_DEMCR |= (1UL << 24);

    /* Start the CPU cycle counter from zero. */
    DWT_CYCCNT = 0U;
    DWT_CTRL |= 1UL;

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");
}

uint32_t timing_cycles(void)
{
    return DWT_CYCCNT;
}

bool timing_has_elapsed(uint32_t start_cycles, uint32_t timeout_ms)
{
    /*
     * Limit each interval so its cycle count fits comfortably
     * within the 32-bit counter range.
     */
    if (timeout_ms > MAX_TIMEOUT_MS)
    {
        return true;
    }

    uint32_t elapsed = timing_cycles() - start_cycles;

    return elapsed >= (timeout_ms * CYCLES_PER_MS);
}

void timing_delay_ms(uint32_t milliseconds)
{
    while (milliseconds != 0U)
    {
        uint32_t interval = milliseconds;

        if (interval > MAX_TIMEOUT_MS)
        {
            interval = MAX_TIMEOUT_MS;
        }

        uint32_t start = timing_cycles();

        while (!timing_has_elapsed(start, interval))
        {
            __asm volatile ("nop");
        }

        milliseconds -= interval;
    }
}