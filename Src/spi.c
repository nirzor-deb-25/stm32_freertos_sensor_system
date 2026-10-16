#include "spi.h"
#include "timing.h"
#include "stm32_regs.h"

#define SENSOR_CS_MASK (1UL << 3)

#define SPI_CR1_CPHA    (1UL << 0)
#define SPI_CR1_CPOL    (1UL << 1)
#define SPI_CR1_MSTR    (1UL << 2)
#define SPI_CR1_SPE     (1UL << 6)
#define SPI_CR1_SSI     (1UL << 8)
#define SPI_CR1_SSM     (1UL << 9)

#define SPI_SR_RXNE    (1UL << 0)
#define SPI_SR_TXE     (1UL << 1)
#define SPI_SR_MODF    (1UL << 5)
#define SPI_SR_OVR     (1UL << 6)
#define SPI_SR_BSY     (1UL << 7)

#define SPI_TIMEOUT_MS 10U

static void sensor_deselect(void)
{
    GPIOE_BSRR = SENSOR_CS_MASK;
}

static void sensor_select(void)
{
    GPIOE_BSRR = SENSOR_CS_MASK << 16;
}

void spi_init(void)
{
    /* Enable GPIOA and GPIOE clocks. */
    RCC_AHB1ENR |= (1UL << 0) | (1UL << 4);
    (void)RCC_AHB1ENR;

    /* Preload CS high before making PE3 an output. */
    sensor_deselect();

    GPIOE_OTYPER &= ~SENSOR_CS_MASK;
    GPIOE_OSPEEDR &= ~(3UL << 6);
    GPIOE_PUPDR &= ~(3UL << 6);

    GPIOE_MODER =
        (GPIOE_MODER & ~(3UL << 6)) |
        (1UL << 6);

    /* Enable and reset SPI1. */
    RCC_APB2ENR |= (1UL << 12);
    (void)RCC_APB2ENR;

    RCC_APB2RSTR |= (1UL << 12);
    RCC_APB2RSTR &= ~(1UL << 12);
    (void)RCC_APB2RSTR;

    /* PA5/PA6/PA7: push-pull, medium speed, no pulls. */
    GPIOA_OTYPER &=
        ~((1UL << 5) | (1UL << 6) | (1UL << 7));

    GPIOA_OSPEEDR =
        (GPIOA_OSPEEDR &
         ~((3UL << 10) | (3UL << 12) | (3UL << 14))) |
        (1UL << 10) |
        (1UL << 12) |
        (1UL << 14);

    GPIOA_PUPDR &=
        ~((3UL << 10) | (3UL << 12) | (3UL << 14));

    /* AF5 selects SPI1 on PA5, PA6 and PA7. */
    GPIOA_AFRL =
        (GPIOA_AFRL &
         ~((15UL << 20) | (15UL << 24) | (15UL << 28))) |
        (5UL << 20) |
        (5UL << 24) |
        (5UL << 28);

    GPIOA_MODER =
        (GPIOA_MODER &
         ~((3UL << 10) | (3UL << 12) | (3UL << 14))) |
        (2UL << 10) |
        (2UL << 12) |
        (2UL << 14);

    /*
     * Master, full duplex, 8-bit, MSB first.
     * Mode 3: CPOL=1, CPHA=1.
     * BR=100: divide 16 MHz by 32 = 500 kHz.
     * Software NSS management; PE3 supplies physical CS.
     */
    SPI1_CR2 = 0U;

    SPI1_CR1 =
        SPI_CR1_MSTR |
        SPI_CR1_CPOL |
        SPI_CR1_CPHA |
        SPI_CR1_SSM |
        SPI_CR1_SSI |
        (4UL << 3);

    SPI1_CR1 |= SPI_CR1_SPE;
}

static spi_status_t wait_status(
    uint32_t mask,
    uint32_t expected,
    uint32_t start
)
{
    for (;;)
    {
        uint32_t status = SPI1_SR;

        if ((status & SPI_SR_MODF) != 0U)
        {
            return SPI_MODE_FAULT;
        }

        if ((status & SPI_SR_OVR) != 0U)
        {
            return SPI_OVERRUN;
        }

        if ((status & mask) == expected)
        {
            return SPI_OK;
        }

        if (timing_has_elapsed(start, SPI_TIMEOUT_MS))
        {
            return SPI_TIMEOUT;
        }
    }
}

static spi_status_t abort_transfer(spi_status_t error)
{
    /* End the failed transaction and reset the SPI controller. */
    sensor_deselect();
    spi_init();

    return error;
}

spi_status_t spi_transfer(
    const uint8_t *tx,
    uint8_t *rx,
    size_t count
)
{
    if (count == 0U)
    {
        return SPI_INVALID_ARGUMENT;
    }

    /*
     * One deadline bounds the entire transaction,
     * including the final wait for the clock to stop.
     */
    uint32_t start = timing_cycles();

    spi_status_t result =
        wait_status(SPI_SR_BSY, 0U, start);

    if (result != SPI_OK)
    {
        return abort_transfer(result);
    }

    sensor_select();

    for (size_t index = 0U; index < count; index++)
    {
        if (timing_has_elapsed(start, SPI_TIMEOUT_MS))
        {
            return abort_transfer(SPI_TIMEOUT);
        }

        result = wait_status(SPI_SR_TXE, SPI_SR_TXE, start);

        if (result != SPI_OK)
        {
            return abort_transfer(result);
        }

        uint8_t outgoing = 0U;

        if (tx != NULL)
        {
            outgoing = tx[index];
        }

        SPI1_DR8 = outgoing;

        result = wait_status(SPI_SR_RXNE, SPI_SR_RXNE, start);

        if (result != SPI_OK)
        {
            return abort_transfer(result);
        }

        /*
         * Every transmitted byte also receives a byte.
         * Always read DR, even when the caller discards it.
         */
        uint8_t incoming = SPI1_DR8;

        if (rx != NULL)
        {
            rx[index] = incoming;
        }
    }

    result = wait_status(SPI_SR_TXE, SPI_SR_TXE, start);

    if (result != SPI_OK)
    {
        return abort_transfer(result);
    }

    result = wait_status(SPI_SR_BSY, 0U, start);

    if (result != SPI_OK)
    {
        return abort_transfer(result);
    }

    /* Keep CS low until the final byte has finished shifting. */
    sensor_deselect();

    return SPI_OK;
}

const char *spi_status_name(spi_status_t status)
{
    switch (status)
    {
        case SPI_OK:
            return "OK";

        case SPI_INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";

        case SPI_TIMEOUT:
            return "TIMEOUT";

        case SPI_MODE_FAULT:
            return "MODE_FAULT";

        case SPI_OVERRUN:
            return "OVERRUN";

        default:
            return "UNKNOWN";
    }
}