#include <stdint.h>

#define SYSTEM_REG32(address) \
    (*(volatile uint32_t *)(uintptr_t)(address))

#define SYSTEM_VTOR  SYSTEM_REG32(0xE000ED08UL)
#define SYSTEM_CPACR SYSTEM_REG32(0xE000ED88UL)

void SystemInit(void)
{
    /* Standalone firmware starts at the beginning of Flash. */
    SYSTEM_VTOR = 0x08000000UL;

    /* Allow access to the Cortex-M4 floating-point unit. */
    SYSTEM_CPACR |= (0xFUL << 20);

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    /* Keep the reset-default 16 MHz HSI clock. */
}

void _init(void)
{
}