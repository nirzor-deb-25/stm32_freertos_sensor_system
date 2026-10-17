#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "uart_task.h"
#include "uart_console.h"
#include "sensor_task.h"
#include "led_task.h"
#include "app_context.h"

#define COMMAND_BUFFER_SIZE 64U
#define RX_BUDGET           64U
#define STREAM_PERIOD_MS    1000U

static void send_sample(const sensor_sample_t *sample)
{
    char text[160];

    int length = snprintf(
        text,
        sizeof(text),
        "SAMPLE seq=%" PRIu32
        " tick=%" PRIu32
        " X=%" PRId32
        " Y=%" PRId32
        " Z=%" PRId32 " mg\r\n",
        sample->sequence,
        (uint32_t)sample->timestamp,
        sample->x_mg,
        sample->y_mg,
        sample->z_mg
    );

    if ((length < 0) || ((size_t)length >= sizeof(text)))
    {
        (void)uart_console_write("ERR OUTPUT\r\n");
        return;
    }

    (void)uart_console_write(text);
}

static void send_status(void)
{
    char text[256];

    int length = snprintf(
        text,
        sizeof(text),
        "STATUS id=0x%02" PRIX32
        " configured=%" PRIu32
        " samples=%" PRIu32
        " sensor_drop=%" PRIu32
        " sensor_err=%" PRIu32
        " rx_drop=%" PRIu32
        " rx_err=%" PRIu32
        " tx_timeout=%" PRIu32
        " heap=%lu\r\n",
        sensor_diagnostics.id,
        sensor_diagnostics.configured,
        sensor_diagnostics.sample_count,
        sensor_diagnostics.dropped_count,
        sensor_diagnostics.error_count,
        uart_diagnostics.dropped_bytes,
        uart_diagnostics.receive_errors,
        uart_diagnostics.transmit_timeouts,
        (unsigned long)xPortGetFreeHeapSize()
    );

    if ((length < 0) || ((size_t)length >= sizeof(text)))
    {
        (void)uart_console_write("ERR OUTPUT\r\n");
        return;
    }

    (void)uart_console_write(text);
}

static void send_led_status(void)
{
    led_state_t state = led_get_state();
    const char *mode_name;

    switch (state.mode)
    {
        case LED_MODE_STEADY:
            mode_name = "STEADY";
            break;

        case LED_MODE_BLINK:
            mode_name = "BLINK";
            break;

        case LED_MODE_BREATH:
            mode_name = "BREATH";
            break;

        default:
            mode_name = "UNKNOWN";
            break;
    }

    char text[160];

    int length = snprintf(
        text,
        sizeof(text),
        "LED brightness=%" PRIu32
        " mode=%s"
        " output=%" PRIu32
        " applied=%" PRIu32
        " rejected=%" PRIu32 "\r\n",
        state.brightness,
        mode_name,
        state.output_percent,
        state.applied_commands,
        state.rejected_commands
    );

    if ((length < 0) || ((size_t)length >= sizeof(text)))
    {
        (void)uart_console_write("ERR OUTPUT\r\n");
        return;
    }

    (void)uart_console_write(text);
}

/* Accept one to three decimal digits, with a value of 0 to 100. */
static bool parse_brightness(const char *text, uint32_t *result)
{
    size_t length = strlen(text);

    if ((length == 0U) || (length > 3U))
    {
        return false;
    }

    uint32_t value = 0U;

    for (size_t index = 0U; index < length; index++)
    {
        if ((text[index] < '0') || (text[index] > '9'))
        {
            return false;
        }

        value = value * 10U + (uint32_t)(text[index] - '0');
    }

    if (value > 100U)
    {
        return false;
    }

    *result = value;
    return true;
}

static void queue_led_command(
    QueueHandle_t led_queue,
    led_command_type_t type,
    uint32_t value)
{
    const led_command_t command = {
        .type = type,
        .value = value
    };

    if (xQueueSend(led_queue, &command, 0U) == pdPASS)
    {
        /*
         * This confirms queue admission.
         * LEDSTATUS reports when the LED task has applied it.
         */
        (void)uart_console_write("OK QUEUED\r\n");
    }
    else
    {
        (void)uart_console_write("ERR LED_QUEUE_FULL\r\n");
    }
}

static void handle_command(
    const char *command,
    QueueHandle_t led_queue,
    bool *streaming,
    bool have_sample,
    const sensor_sample_t *latest)
{
    if (strcmp(command, "PING") == 0)
    {
        (void)uart_console_write("PONG\r\n");
    }
    else if (strcmp(command, "SAMPLE") == 0)
    {
        if (have_sample)
        {
            send_sample(latest);
        }
        else
        {
            (void)uart_console_write("ERR NO_SAMPLE\r\n");
        }
    }
    else if (strcmp(command, "STATUS") == 0)
    {
        send_status();
    }
    else if (strcmp(command, "LEDSTATUS") == 0)
    {
        send_led_status();
    }
    else if (strncmp(command, "BRIGHTNESS ", 11U) == 0)
    {
        uint32_t brightness;

        if (!parse_brightness(command + 11U, &brightness))
        {
            (void)uart_console_write("ERR BRIGHTNESS\r\n");
        }
        else
        {
            queue_led_command(
                led_queue,
                LED_COMMAND_BRIGHTNESS,
                brightness
            );
        }
    }
    else if (strcmp(command, "MODE STEADY") == 0)
    {
        queue_led_command(
            led_queue,
            LED_COMMAND_MODE,
            LED_MODE_STEADY
        );
    }
    else if (strcmp(command, "MODE BLINK") == 0)
    {
        queue_led_command(
            led_queue,
            LED_COMMAND_MODE,
            LED_MODE_BLINK
        );
    }
    else if (strcmp(command, "MODE BREATH") == 0)
    {
        queue_led_command(
            led_queue,
            LED_COMMAND_MODE,
            LED_MODE_BREATH
        );
    }
    else if (strcmp(command, "STREAM ON") == 0)
    {
        *streaming = true;
        (void)uart_console_write("OK STREAM ON\r\n");
    }
    else if (strcmp(command, "STREAM OFF") == 0)
    {
        *streaming = false;
        (void)uart_console_write("OK STREAM OFF\r\n");
    }
    else if (strcmp(command, "HELP") == 0)
    {
        (void)uart_console_write(
            "COMMANDS: PING, SAMPLE, STATUS, LEDSTATUS, "
            "BRIGHTNESS 0..100, MODE STEADY|BLINK|BREATH, "
            "STREAM ON, STREAM OFF, HELP\r\n"
        );
    }
    else
    {
        (void)uart_console_write("ERR COMMAND\r\n");
    }
}

void uart_task(void *argument)
{
    const app_context_t *context = (const app_context_t *)argument;

    configASSERT(context != 0);
    configASSERT(context->sample_queue != 0);
    configASSERT(context->led_queue != 0);

    uart_console_init();

    char command[COMMAND_BUFFER_SIZE];
    uint32_t command_length = 0U;

    bool discard_line = false;
    bool streaming = false;
    bool have_sample = false;

    sensor_sample_t latest = {0};

    TickType_t previous_report = xTaskGetTickCount();

    (void)uart_console_write("READY FREERTOS SENSOR\r\n");

    for (;;)
    {
        /* Keep consuming sensor samples when streaming is disabled. */
        for (uint32_t count = 0U; count < 8U; count++)
        {
            sensor_sample_t sample;

            if (xQueueReceive(
                    context->sample_queue,
                    &sample,
                    0U) != pdPASS)
            {
                break;
            }

            latest = sample;
            have_sample = true;
        }

        /* Bound RX processing so sample handling also gets CPU time. */
        for (uint32_t count = 0U; count < RX_BUDGET; count++)
        {
            uint8_t value;

            if (!uart_console_read(&value))
            {
                break;
            }

            if (value == '\r')
            {
                continue;
            }

            if (value == '\n')
            {
                if (discard_line)
                {
                    (void)uart_console_write("ERR LINE\r\n");
                }
                else if (command_length != 0U)
                {
                    command[command_length] = '\0';

                    handle_command(
                        command,
                        context->led_queue,
                        &streaming,
                        have_sample,
                        &latest
                    );
                }

                command_length = 0U;
                discard_line = false;
                continue;
            }

            if (discard_line)
            {
                continue;
            }

            if ((value < 32U) ||
                (value > 126U) ||
                (command_length >= COMMAND_BUFFER_SIZE - 1U))
            {
                discard_line = true;
                continue;
            }

            command[command_length] = (char)value;
            command_length++;
        }

        TickType_t now = xTaskGetTickCount();

        if ((TickType_t)(now - previous_report) >=
            pdMS_TO_TICKS(STREAM_PERIOD_MS))
        {
            previous_report = now;

            if (streaming && have_sample)
            {
                send_sample(&latest);
            }
        }

        /*
         * The UART RX ISR gives a binary semaphore.
         * A timeout allows regular sample queue processing.
         */
        (void)uart_console_wait(pdMS_TO_TICKS(10U));
    }
}