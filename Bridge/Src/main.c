#include <stdbool.h>
#include <stdint.h>

#define REG32(address) \
    (*(volatile uint32_t *)(uintptr_t)(address))

#define RCC_AHB1ENR REG32(0x40023830UL)
#define RCC_APB1ENR REG32(0x40023840UL)
#define RCC_APB1RSTR REG32(0x40023820UL)

#define GPIOB_BASE 0x40020400UL
#define GPIOC_BASE 0x40020800UL
#define GPIOD_BASE 0x40020C00UL

#define USART3_BASE 0x40004800UL
#define UART4_BASE  0x40004C00UL

#define UART_SR(base)  REG32((base) + 0x00UL)
#define UART_DR(base)  REG32((base) + 0x04UL)
#define UART_BRR(base) REG32((base) + 0x08UL)
#define UART_CR1(base) REG32((base) + 0x0CUL)
#define UART_CR2(base) REG32((base) + 0x10UL)
#define UART_CR3(base) REG32((base) + 0x14UL)

#define UART_RXNE       (1UL << 5)
#define UART_TXE        (1UL << 7)
#define UART_ERRORS     0x0FUL

#define RING_SIZE       256U
#define RING_MASK       (RING_SIZE - 1U)

#define DWT_CYCCNT      REG32(0xE0001004UL)

typedef struct
{
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint8_t bytes[RING_SIZE];
} byte_ring_t;

typedef struct
{
    uint32_t pc_to_sensor;
    uint32_t sensor_to_pc;
    uint32_t receive_errors;
    uint32_t queue_drops;
} bridge_diagnostics_t;

volatile bridge_diagnostics_t bridge_diagnostics = {0};

static byte_ring_t pc_rx;
static byte_ring_t sensor_rx;

static void ring_push(byte_ring_t *ring, uint8_t value)
{
    uint32_t head = ring->head;
    uint32_t next = (head + 1U) & RING_MASK;

    if (next == ring->tail)
    {
        bridge_diagnostics.queue_drops++;
        return;
    }

    ring->bytes[head] = value;

    __asm volatile ("dmb" ::: "memory");

    ring->head = next;
}

static bool ring_pop(byte_ring_t *ring, uint8_t *value)
{
    uint32_t tail = ring->tail;

    if (tail == ring->head)
    {
        return false;
    }

    __asm volatile ("dmb" ::: "memory");

    *value = ring->bytes[tail];

    __asm volatile ("dmb" ::: "memory");

    ring->tail = (tail + 1U) & RING_MASK;

    return true;
}

static void receive_interrupt(uint32_t base, byte_ring_t *ring)
{
    uint32_t status = UART_SR(base);

    if ((status & (UART_RXNE | UART_ERRORS)) == 0U)
    {
        return;
    }

    /* SR followed by DR clears receive/error flags. */
    uint8_t value = (uint8_t)UART_DR(base);

    if ((status & UART_ERRORS) != 0U)
    {
        bridge_diagnostics.receive_errors++;
        return;
    }

    if ((status & UART_RXNE) != 0U)
    {
        ring_push(ring, value);
    }
}

void USART3_IRQHandler(void)
{
    receive_interrupt(USART3_BASE, &pc_rx);
}

void UART4_IRQHandler(void)
{
    receive_interrupt(UART4_BASE, &sensor_rx);
}

/* Both pins supplied here are in the GPIO AFRH register. */
static void configure_uart_pins(
    uint32_t base,
    uint32_t tx,
    uint32_t rx,
    uint32_t alternate)
{
    uint32_t mode_mask =
        (3UL << (tx * 2U)) |
        (3UL << (rx * 2U));

    uint32_t tx_af_shift = (tx - 8U) * 4U;
    uint32_t rx_af_shift = (rx - 8U) * 4U;

    REG32(base + 0x04UL) &= ~((1UL << tx) | (1UL << rx));

    REG32(base + 0x08UL) =
        (REG32(base + 0x08UL) & ~mode_mask) |
        (1UL << (tx * 2U)) |
        (1UL << (rx * 2U));

    /* RX pull-up; TX has no pull resistor. */
    REG32(base + 0x0CUL) =
        (REG32(base + 0x0CUL) & ~mode_mask) |
        (1UL << (rx * 2U));

    REG32(base + 0x24UL) =
        (REG32(base + 0x24UL) &
         ~((15UL << tx_af_shift) | (15UL << rx_af_shift))) |
        (alternate << tx_af_shift) |
        (alternate << rx_af_shift);

    REG32(base) =
        (REG32(base) & ~mode_mask) |
        (2UL << (tx * 2U)) |
        (2UL << (rx * 2U));
}

static void configure_uart(uint32_t base)
{
    UART_CR1(base) = 0U;
    UART_CR2(base) = 0U;
    UART_CR3(base) = 0U;

    /* 16 MHz APB1, oversampling by 16, 115200 baud. */
    UART_BRR(base) = 139U;

    (void)UART_SR(base);
    (void)UART_DR(base);

    /* UE, RXNE interrupt, transmitter and receiver. */
    UART_CR1(base) =
        (1UL << 13) |
        (1UL << 5) |
        (1UL << 3) |
        (1UL << 2);
}

int main(void)
{
    /* GPIOB, GPIOC and GPIOD. */
    RCC_AHB1ENR |= (1UL << 1) | (1UL << 2) | (1UL << 3);
    (void)RCC_AHB1ENR;

    /* USART3 and UART4. */
    RCC_APB1ENR |= (1UL << 18) | (1UL << 19);
    (void)RCC_APB1ENR;

    RCC_APB1RSTR |= (1UL << 18) | (1UL << 19);
    RCC_APB1RSTR &= ~((1UL << 18) | (1UL << 19));

    configure_uart_pins(GPIOD_BASE, 8U, 9U, 7U);
    configure_uart_pins(GPIOC_BASE, 10U, 11U, 8U);

    /* Blue LD2, PB7: bridge heartbeat. */
    REG32(GPIOB_BASE + 0x18UL) = 1UL << 23;
    REG32(GPIOB_BASE + 0x04UL) &= ~(1UL << 7);
    REG32(GPIOB_BASE + 0x08UL) &= ~(3UL << 14);
    REG32(GPIOB_BASE + 0x0CUL) &= ~(3UL << 14);

    REG32(GPIOB_BASE) =
        (REG32(GPIOB_BASE) & ~(3UL << 14)) |
        (1UL << 14);

    /* Enable the CPU cycle counter. */
    REG32(0xE000EDFCUL) |= 1UL << 24;
    DWT_CYCCNT = 0U;
    REG32(0xE0001000UL) |= 1UL;

    configure_uart(USART3_BASE);
    configure_uart(UART4_BASE);

    /* IRQ39 = USART3; IRQ52 = UART4. Both in NVIC group 1. */
    REG32(0xE000E284UL) = (1UL << 7) | (1UL << 20);
    REG32(0xE000E104UL) = (1UL << 7) | (1UL << 20);

    __asm volatile ("cpsie i" ::: "memory");

    uint32_t previous_toggle = DWT_CYCCNT;
    bool blue_on = false;

    for (;;)
    {
        uint8_t value;

        if (((UART_SR(UART4_BASE) & UART_TXE) != 0U) &&
            ring_pop(&pc_rx, &value))
        {
            UART_DR(UART4_BASE) = value;
            bridge_diagnostics.pc_to_sensor++;
        }

        if (((UART_SR(USART3_BASE) & UART_TXE) != 0U) &&
            ring_pop(&sensor_rx, &value))
        {
            UART_DR(USART3_BASE) = value;
            bridge_diagnostics.sensor_to_pc++;
        }

        uint32_t now = DWT_CYCCNT;

        if ((uint32_t)(now - previous_toggle) >= 4000000UL)
        {
            previous_toggle = now;
            blue_on = !blue_on;

            REG32(GPIOB_BASE + 0x18UL) = blue_on
                ? (1UL << 7)
                : (1UL << 23);
        }
    }
}