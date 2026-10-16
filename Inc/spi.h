#ifndef SPI_H
#define SPI_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    SPI_OK = 0,
    SPI_INVALID_ARGUMENT,
    SPI_TIMEOUT,
    SPI_MODE_FAULT,
    SPI_OVERRUN
} spi_status_t;

void spi_init(void);

/*
 * One transaction with the onboard sensor:
 * CS goes low, all bytes transfer, then CS goes high.
 *
 * tx == NULL: transmit zero bytes.
 * rx == NULL: discard received bytes.
 *
 * Call timing_init() and spi_init() before using this function.
 * This blocking driver must not be called concurrently.
 */
spi_status_t spi_transfer(
    const uint8_t *tx,
    uint8_t *rx,
    size_t count
);

const char *spi_status_name(spi_status_t status);

#endif