#include "sensor_task.h"

#include "task.h"
#include "lis3dsh.h"
#include "spi.h"
#include "timing.h"

#define SENSOR_PERIOD_MS 100U
#define SENSOR_RETRY_MS  1000U

volatile sensor_diagnostics_t sensor_diagnostics = {0};

static bool spi_succeeded(spi_status_t status)
{
    sensor_diagnostics.spi_status = (uint32_t)status;

    return status == SPI_OK;
}

static bool sensor_initialize(void)
{
    uint8_t id = 0U;

    sensor_diagnostics.configured = 0U;

    if (!spi_succeeded(lis3dsh_read_id(&id)))
    {
        return false;
    }

    sensor_diagnostics.id = id;

    if (id != LIS3DSH_EXPECTED_ID)
    {
        return false;
    }

    if (!spi_succeeded(lis3dsh_configure()))
    {
        return false;
    }

    static const uint8_t registers[] = {
        LIS3DSH_REG_CTRL4,
        LIS3DSH_REG_CTRL5,
        LIS3DSH_REG_CTRL6
    };

    static const uint8_t expected[] = {
        LIS3DSH_CTRL4_VALUE,
        LIS3DSH_CTRL5_VALUE,
        LIS3DSH_CTRL6_VALUE
    };

    for (uint32_t index = 0U; index < 3U; index++)
    {
        uint8_t value = 0U;

        if (!spi_succeeded(lis3dsh_read_register(
                registers[index],
                &value)))
        {
            return false;
        }

        if (value != expected[index])
        {
            return false;
        }
    }

    sensor_diagnostics.configured = 1U;

    return true;
}

void sensor_task(void *argument)
{
    QueueHandle_t sample_queue = (QueueHandle_t)argument;

    configASSERT(sample_queue != 0);

    /*
     * Initialize the cycle counter once.
     * It supplies short SPI transaction timeouts.
     */
    timing_init();
    spi_init();

    /* Allow the sensor to finish powering up. */
    vTaskDelay(pdMS_TO_TICKS(100U));

    uint32_t sequence = 0U;

    for (;;)
    {
        if (!sensor_initialize())
        {
            sensor_diagnostics.error_count++;

            vTaskDelay(pdMS_TO_TICKS(SENSOR_RETRY_MS));
            continue;
        }

        TickType_t previous_wake = xTaskGetTickCount();

        for (;;)
        {
            uint8_t status = 0U;

            if (!spi_succeeded(lis3dsh_read_register(
                    LIS3DSH_REG_STATUS,
                    &status)))
            {
                sensor_diagnostics.error_count++;
                break;
            }

            if ((status & LIS3DSH_STATUS_DATA_READY) != 0U)
            {
                lis3dsh_raw_t raw;

                if (!spi_succeeded(lis3dsh_read_xyz(&raw)))
                {
                    sensor_diagnostics.error_count++;
                    break;
                }

                sequence++;
                sensor_diagnostics.sample_count++;

                sensor_sample_t sample = {
                    .sequence = sequence,
                    .timestamp = xTaskGetTickCount(),
                    .x_mg = lis3dsh_raw_to_mg(raw.x),
                    .y_mg = lis3dsh_raw_to_mg(raw.y),
                    .z_mg = lis3dsh_raw_to_mg(raw.z)
                };

                /*
                 * The queue copies the complete sample.
                 * Never block sensor acquisition on a full queue.
                 */
                if (xQueueSend(
                        sample_queue,
                        &sample,
                        0U) != pdPASS)
                {
                    sensor_diagnostics.dropped_count++;
                }
            }
            else
            {
                sensor_diagnostics.not_ready_count++;
            }

            (void)xTaskDelayUntil(
                &previous_wake,
                pdMS_TO_TICKS(SENSOR_PERIOD_MS)
            );
        }

        /*
         * A communication failure triggers a delayed
         * reinitialization attempt. Other tasks keep running.
         */
        sensor_diagnostics.configured = 0U;

        vTaskDelay(pdMS_TO_TICKS(SENSOR_RETRY_MS));
    }
}