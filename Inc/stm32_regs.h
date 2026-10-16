#ifndef STM32_REGS_H
#define STM32_REGS_H

#include <stdint.h>

#define REG32(address) \
    (*(volatile uint32_t *)(uintptr_t)(address))

#define REG8(address) \
    (*(volatile uint8_t *)(uintptr_t)(address))

/* Reset and clock control */
#define RCC_AHB1ENR     REG32(0x40023830UL)
#define RCC_APB2ENR     REG32(0x40023844UL)
#define RCC_APB2RSTR    REG32(0x40023824UL)

/* GPIOA: SPI1 on PA5, PA6 and PA7 */
#define GPIOA_MODER    REG32(0x40020000UL)
#define GPIOA_OTYPER   REG32(0x40020004UL)
#define GPIOA_OSPEEDR  REG32(0x40020008UL)
#define GPIOA_PUPDR    REG32(0x4002000CUL)
#define GPIOA_IDR      REG32(0x40020010UL)
#define GPIOA_BSRR     REG32(0x40020018UL)
#define GPIOA_AFRL     REG32(0x40020020UL)

/* GPIOD: onboard LEDs on PD12 through PD15 */
#define GPIOD_MODER    REG32(0x40020C00UL)
#define GPIOD_OTYPER   REG32(0x40020C04UL)
#define GPIOD_OSPEEDR  REG32(0x40020C08UL)
#define GPIOD_PUPDR    REG32(0x40020C0CUL)
#define GPIOD_ODR      REG32(0x40020C14UL)
#define GPIOD_BSRR     REG32(0x40020C18UL)

/* GPIOE: LIS3DSH chip select on PE3 */
#define GPIOE_MODER    REG32(0x40021000UL)
#define GPIOE_OTYPER   REG32(0x40021004UL)
#define GPIOE_OSPEEDR  REG32(0x40021008UL)
#define GPIOE_PUPDR    REG32(0x4002100CUL)
#define GPIOE_BSRR     REG32(0x40021018UL)

/* SPI1 */
#define SPI1_CR1       REG32(0x40013000UL)
#define SPI1_CR2       REG32(0x40013004UL)
#define SPI1_SR        REG32(0x40013008UL)

/* Byte access for our 8-bit SPI transfers. */
#define SPI1_DR8       REG8(0x4001300CUL)

/* Cortex-M4 vector table */
#define SCB_VTOR       REG32(0xE000ED08UL)

/* Cortex-M4 cycle counter: delays and bounded waits */
#define CORE_DEMCR     REG32(0xE000EDFCUL)
#define DWT_CTRL       REG32(0xE0001000UL)
#define DWT_CYCCNT     REG32(0xE0001004UL)

#endif