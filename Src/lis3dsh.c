#include "lis3dsh.h"

#define LIS3DSH_WHO_AM_I_REG 0x0FU
#define LIS3DSH_SPI_READ     0x80U
#define LIS3DSH_OUT_X_L_REG  0x28U

static spi_status_t write_register(uint8_t address, uint8_t value)
{
    if (address > 0x7FU)
    {
        return SPI_INVALID_ARGUMENT;
    }

    const uint8_t tx[2] = {
        address,
        value
    };

    return spi_transfer(tx, NULL, sizeof(tx));
}

spi_status_t lis3dsh_read_register(
    uint8_t address,
    uint8_t *value
)
{
    if ((value == NULL) || (address > 0x7FU))
    {
        return SPI_INVALID_ARGUMENT;
    }

    const uint8_t tx[2] = {
        LIS3DSH_SPI_READ | address,
        0U
    };

    uint8_t rx[2] = {0U, 0U};

    spi_status_t result = spi_transfer(tx, rx, sizeof(tx));

    if (result == SPI_OK)
    {
        *value = rx[1];
    }

    return result;
}

spi_status_t lis3dsh_read_id(uint8_t *id)
{
    return lis3dsh_read_register(LIS3DSH_WHO_AM_I_REG, id);
}

spi_status_t lis3dsh_configure(void)
{
    /* Power down measurements while configuring. */
    spi_status_t result =
        write_register(LIS3DSH_REG_CTRL4, 0x07U);

    if (result != SPI_OK)
    {
        return result;
    }

    /* Disable programmable state machine 1. */
    result = write_register(0x21U, 0U);

    if (result != SPI_OK)
    {
        return result;
    }

    /* Disable programmable state machine 2. */
    result = write_register(0x22U, 0U);

    if (result != SPI_OK)
    {
        return result;
    }

    /* Disable sensor interrupt outputs. */
    result = write_register(0x23U, 0U);

    if (result != SPI_OK)
    {
        return result;
    }

    /*
     * CTRL_REG5:
     * 50 Hz filter bandwidth.
     * +/-2 g range.
     * Self-test disabled.
     * Four-wire SPI.
     */
    result = write_register(
        LIS3DSH_REG_CTRL5,
        LIS3DSH_CTRL5_VALUE
    );

    if (result != SPI_OK)
    {
        return result;
    }

    /*
     * CTRL_REG6:
     * Automatic register-address increment enabled.
     * FIFO disabled.
     */
    result = write_register(
        LIS3DSH_REG_CTRL6,
        LIS3DSH_CTRL6_VALUE
    );

    if (result != SPI_OK)
    {
        return result;
    }

    /*
     * CTRL_REG4:
     * 100 Hz output data rate.
     * Block data update enabled.
     * X, Y and Z axes enabled.
     */
    return write_register(
        LIS3DSH_REG_CTRL4,
        LIS3DSH_CTRL4_VALUE
    );
}

static int16_t decode_axis(uint8_t low, uint8_t high)
{
    int32_t value =
        (int32_t)low |
        ((int32_t)high << 8);

    if (value >= 32768L)
    {
        value -= 65536L;
    }

    return (int16_t)value;
}

spi_status_t lis3dsh_read_xyz(lis3dsh_raw_t *sample)
{
    if (sample == NULL)
    {
        return SPI_INVALID_ARGUMENT;
    }

    /*
     * One command followed by six dummy bytes.
     * The sensor returns X low/high, Y low/high, Z low/high.
     * CTRL_REG6 enables automatic address increment.
     */
    const uint8_t tx[7] = {
        LIS3DSH_SPI_READ | LIS3DSH_OUT_X_L_REG,
        0U, 0U, 0U, 0U, 0U, 0U
    };

    uint8_t rx[7] = {0U};

    spi_status_t result = spi_transfer(tx, rx, sizeof(tx));

    if (result != SPI_OK)
    {
        return result;
    }

    lis3dsh_raw_t decoded;

    decoded.x = decode_axis(rx[1], rx[2]);
    decoded.y = decode_axis(rx[3], rx[4]);
    decoded.z = decode_axis(rx[5], rx[6]);

    *sample = decoded;

    return SPI_OK;
}

int32_t lis3dsh_raw_to_mg(int16_t raw)
{
    /*
     * Nominal sensitivity at +/-2 g: 0.06 mg/count.
     * 1000 mg = 1 g.
     */
    return ((int32_t)raw * 6L) / 100L;
}