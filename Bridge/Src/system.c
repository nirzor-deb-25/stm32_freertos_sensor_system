#include <stdint.h>

void SystemInit(void)
{
    /* Standalone bridge firmware, reset-default 16 MHz HSI. */
    (*(volatile uint32_t *)0xE000ED08UL) = 0x08000000UL;

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");
}

void _init(void)
{
}