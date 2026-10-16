#ifndef LIS3DSH_H
#define LIS3DSH_H

#include <stdint.h>
#include "spi.h"

#define LIS3DSH_EXPECTED_ID 0x3FU

#define LIS3DSH_REG_CTRL4  0x20U
#define LIS3DSH_REG_CTRL5  0x24U
#define LIS3DSH_REG_CTRL6  0x25U
#define LIS3DSH_REG_STATUS 0x27U

#define LIS3DSH_STATUS_DATA_READY (1U << 3)

#define LIS3DSH_CTRL4_VALUE 0x6FU
#define LIS3DSH_CTRL5_VALUE 0xC0U
#define LIS3DSH_CTRL6_VALUE 0x10U

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} lis3dsh_raw_t;

spi_status_t lis3dsh_read_id(uint8_t *id);

spi_status_t lis3dsh_read_register(
    uint8_t address,
    uint8_t *value
);

/* Call after confirming the device identification. */
spi_status_t lis3dsh_configure(void);

/* Read the latest output registers; does not wait for new data. */
spi_status_t lis3dsh_read_xyz(lis3dsh_raw_t *sample);

/* Conversion for our configured +/-2 g range only. */
int32_t lis3dsh_raw_to_mg(int16_t raw);

#endif