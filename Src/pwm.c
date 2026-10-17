#include "pwm.h"
#include "stm32_regs.h"

#define PWM_GPIO_MODER   REG32(0x40020C00UL)
#define PWM_GPIO_OTYPER  REG32(0x40020C04UL)
#define PWM_GPIO_SPEED   REG32(0x40020C08UL)
#define PWM_GPIO_PUPDR   REG32(0x40020C0CUL)
#define PWM_GPIO_BSRR    REG32(0x40020C18UL)
#define PWM_GPIO_AFRH    REG32(0x40020C24UL)

#define PWM_TIM_CR1      REG32(0x40000800UL)
#define PWM_TIM_DIER     REG32(0x4000080CUL)
#define PWM_TIM_SR       REG32(0x40000810UL)
#define PWM_TIM_EGR      REG32(0x40000814UL)
#define PWM_TIM_CCMR1    REG32(0x40000818UL)
#define PWM_TIM_CCER     REG32(0x40000820UL)
#define PWM_TIM_CNT      REG32(0x40000824UL)
#define PWM_TIM_PSC      REG32(0x40000828UL)
#define PWM_TIM_ARR      REG32(0x4000082CUL)
#define PWM_TIM_CCR1     REG32(0x40000834UL)

#define PWM_PERIOD_COUNTS 1000U

void pwm_init(void)
{
    /* Enable GPIOD. */
    REG32(0x40023830UL) |= (1UL << 3);
    (void)REG32(0x40023830UL);

    /* Enable and reset TIM4. */
    REG32(0x40023840UL) |= (1UL << 2);
    (void)REG32(0x40023840UL);

    REG32(0x40023820UL) |= (1UL << 2);
    REG32(0x40023820UL) &= ~(1UL << 2);

    /* PD12 initially low, push-pull, low speed, no pulls. */
    PWM_GPIO_BSRR = (1UL << 28);

    PWM_GPIO_OTYPER &= ~(1UL << 12);
    PWM_GPIO_SPEED &= ~(3UL << 24);
    PWM_GPIO_PUPDR &= ~(3UL << 24);

    /* PD12 AF2: TIM4 channel 1. */
    PWM_GPIO_AFRH =
        (PWM_GPIO_AFRH & ~(15UL << 16)) |
        (2UL << 16);

    PWM_TIM_CR1 = 0U;
    PWM_TIM_DIER = 0U;
    PWM_TIM_CCER = 0U;

    /*
     * Timer clock = 16 MHz.
     * Prescaler: 16 MHz / 16 = 1 MHz.
     * Period: 1000 counts = 1 kHz PWM.
     */
    PWM_TIM_PSC = 16U - 1U;
    PWM_TIM_ARR = PWM_PERIOD_COUNTS - 1U;
    PWM_TIM_CCR1 = 0U;
    PWM_TIM_CNT = 0U;

    /*
     * Channel 1 output:
     * PWM mode 1, compare preload enabled.
     */
    PWM_TIM_CCMR1 = (6UL << 4) | (1UL << 3);

    /* Enable auto-reload preload. */
    PWM_TIM_CR1 = (1UL << 7);

    /* Load buffered prescaler and compare values. */
    PWM_TIM_EGR = 1U;
    PWM_TIM_SR = 0U;

    /* Enable channel 1, active high. */
    PWM_TIM_CCER = 1U;

    /* Connect PD12 to the timer. */
    PWM_GPIO_MODER =
        (PWM_GPIO_MODER & ~(3UL << 24)) |
        (2UL << 24);

    /* Start the counter. */
    PWM_TIM_CR1 |= 1U;
}

bool pwm_set_percent(uint32_t percent)
{
    if (percent > 100U)
    {
        return false;
    }

    /*
     * CCR1 = 0: always off.
     * CCR1 = 1000 with ARR = 999: always on.
     * Preload applies changes at the next timer update.
     */
    PWM_TIM_CCR1 = percent * (PWM_PERIOD_COUNTS / 100U);

    return true;
}