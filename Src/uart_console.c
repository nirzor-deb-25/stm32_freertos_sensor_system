#include "uart_console.h"

#include "task.h"
#include "semphr.h"
#include "stm32_regs.h"

#define CONSOLE_SR   REG32(0x40004400UL)
#define CONSOLE_DR   REG32(0x40004404UL)
#define CONSOLE_BRR  REG32(0x40004408UL)
#define CONSOLE_CR1  REG32(0x4000440CUL)
#define CONSOLE_CR2  REG32(0x40004410UL)
#define CONSOLE_CR3  REG32(0x40004414UL)

#define RXNE         (1UL << 5)
#define TXE          (1UL << 7)
#define RX_ERRORS    0x0FUL

#define RX_SIZE      256U
#define RX_MASK      (RX_SIZE - 1U)

static SemaphoreHandle_t rx_event;

static volatile uint8_t rx_buffer[RX_SIZE];
static volatile uint32_t rx_head;
static volatile uint32_t rx_tail;

volatile uart_diagnostics_t uart_diagnostics = {0};

void uart_console_init(void)
{
    configASSERT(
        xTaskGetSchedulerState() == taskSCHEDULER_RUNNING
    );

    configASSERT(rx_event == 0);

    rx_event = xSemaphoreCreateBinary();
    configASSERT(rx_event != 0);

    /* GPIOA clock. */
    REG32(0x40023830UL) |= 1UL;
    (void)REG32(0x40023830UL);

    /* USART2 clock and peripheral reset. */
    REG32(0x40023840UL) |= (1UL << 17);
    (void)REG32(0x40023840UL);

    REG32(0x40023820UL) |= (1UL << 17);
    REG32(0x40023820UL) &= ~(1UL << 17);

    /* PA2 TX and PA3 RX: push-pull, medium speed. */
    REG32(0x40020004UL) &= ~((1UL << 2) | (1UL << 3));

    REG32(0x40020008UL) =
        (REG32(0x40020008UL) & ~((3UL << 4) | (3UL << 6))) |
        (1UL << 4) |
        (1UL << 6);

    /* Pull up RX; no pull on TX. */
    REG32(0x4002000CUL) =
        (REG32(0x4002000CUL) & ~((3UL << 4) | (3UL << 6))) |
        (1UL << 6);

    /* PA2/PA3 AF7: USART2. */
    REG32(0x40020020UL) =
        (REG32(0x40020020UL) & ~((15UL << 8) | (15UL << 12))) |
        (7UL << 8) |
        (7UL << 12);

    REG32(0x40020000UL) =
        (REG32(0x40020000UL) & ~((3UL << 4) | (3UL << 6))) |
        (2UL << 4) |
        (2UL << 6);

    CONSOLE_CR1 = 0U;
    CONSOLE_CR2 = 0U;
    CONSOLE_CR3 = 0U;

    /* 16 MHz APB1, 115200 baud, oversampling by 16. */
    CONSOLE_BRR = 139U;

    (void)CONSOLE_SR;
    (void)CONSOLE_DR;

    /*
     * USART2 IRQ38, logical priority 6.
     * This is compatible with our FreeRTOS FromISR limit of 5.
     */
    (*(volatile uint8_t *)(uintptr_t)(0xE000E400UL + 38U)) =
        (uint8_t)(6U << 4);

    /* Clear pending IRQ38. */
    REG32(0xE000E284UL) = (1UL << 6);

    /* Enable UART, RXNE interrupt, transmitter and receiver. */
    CONSOLE_CR1 =
        (1UL << 13) |
        (1UL << 5) |
        (1UL << 3) |
        (1UL << 2);

    /* Enable IRQ38 in NVIC group 1. */
    REG32(0xE000E104UL) = (1UL << 6);
}

void USART2_IRQHandler(void)
{
    uint32_t status = CONSOLE_SR;

    if ((status & (RXNE | RX_ERRORS)) == 0U)
    {
        return;
    }

    /* Reading SR followed by DR clears receive/error flags. */
    uint8_t value = (uint8_t)CONSOLE_DR;

    if ((status & RX_ERRORS) != 0U)
    {
        uart_diagnostics.receive_errors++;
    }
    else if ((status & RXNE) != 0U)
    {
        uint32_t head = rx_head;
        uint32_t next = (head + 1U) & RX_MASK;

        uart_diagnostics.received_bytes++;

        if (next == rx_tail)
        {
            uart_diagnostics.dropped_bytes++;
        }
        else
        {
            rx_buffer[head] = value;

            __asm volatile ("dmb" ::: "memory");

            rx_head = next;
        }
    }

    BaseType_t higher_priority_woken = pdFALSE;

    (void)xSemaphoreGiveFromISR(
        rx_event,
        &higher_priority_woken
    );

    portYIELD_FROM_ISR(higher_priority_woken);
}

bool uart_console_wait(TickType_t timeout)
{
    configASSERT(rx_event != 0);

    return xSemaphoreTake(rx_event, timeout) == pdTRUE;
}

bool uart_console_read(uint8_t *value)
{
    configASSERT(value != 0);

    bool available = false;

    taskENTER_CRITICAL();

    if (rx_tail != rx_head)
    {
        *value = rx_buffer[rx_tail];
        rx_tail = (rx_tail + 1U) & RX_MASK;
        available = true;
    }

    taskEXIT_CRITICAL();

    return available;
}

bool uart_console_write(const char *text)
{
    configASSERT(text != 0);

    while (*text != '\0')
    {
        TickType_t start = xTaskGetTickCount();

        while ((CONSOLE_SR & TXE) == 0U)
        {
            if ((TickType_t)(xTaskGetTickCount() - start) >=
                pdMS_TO_TICKS(20U))
            {
                uart_diagnostics.transmit_timeouts++;
                return false;
            }
        }

        CONSOLE_DR = (uint8_t)*text;
        text++;
    }

    return true;
}