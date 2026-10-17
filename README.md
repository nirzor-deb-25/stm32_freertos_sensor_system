# STM32 FreeRTOS Sensor System

A FreeRTOS application for the STM32F407G-DISC1 that combines an onboard SPI accelerometer, UART commands, and PWM LED control.

The peripheral drivers use direct register access without STM32 HAL. FreeRTOS provides task scheduling, queues, and a binary semaphore.

## Hardware

- STM32F407G-DISC1: runs the FreeRTOS application.
- Onboard LIS3DSH: measures acceleration through SPI1.
- NUCLEO-F439ZI: bridges the PC virtual COM port to the Discovery UART.
- Three jumper wires for TX, RX, and common ground.

Both boards are powered through their own ST-LINK USB connections.

## Wiring

| NUCLEO-F439ZI | STM32F407G-DISC1 | Purpose |
|---|---|---|
| PC10, CN11 pin 1, UART4 TX | PA3, P1 pin 13, USART2 RX | Commands to Discovery |
| PC11, CN11 pin 2, UART4 RX | PA2, P1 pin 14, USART2 TX | Reports to PC |
| GND | GND | Common signal reference |

Do not connect the boards' 3.3 V or 5 V supply pins together.

The LIS3DSH is already connected on the Discovery board:

| Signal | STM32F407 pin |
|---|---|
| Chip select | PE3 |
| SPI1 clock | PA5 |
| SPI1 MISO | PA6 |
| SPI1 MOSI | PA7 |

## Software architecture

| Task | Priority | Stack allocation | Responsibility |
|---|---:|---:|---|
| Sensor | 2 | 384 words | Configure LIS3DSH, check data readiness, read acceleration |
| UART | 1 | 768 words | Consume samples, parse commands, transmit reports |
| LED | 1 | 384 words | Process LED commands and update PWM |
| Blue | 1 | 256 words | Toggle the heartbeat LED |

Stack allocations are expressed in 32-bit words. These are configured allocations, not measured stack usage.

The scheduler uses a 1 ms tick and preemption. The CPU runs at the reset-default 16 MHz HSI clock.

### Queues and semaphore

- Sensor sample queue: eight entries, from the Sensor task to the UART task.
- LED command queue: eight entries, from the UART task to the LED task.
- UART binary semaphore: given by the USART2 receive ISR to wake the UART task.
- UART receive ring: 256 storage bytes, with 255 usable entries.
- LED status: copied under a critical section for a consistent snapshot.

The UART task also wakes after a short timeout to consume sensor samples.

The Sensor task exclusively owns the SPI driver. The LED task exclusively owns PWM updates. UART transmission is handled by the UART task.

## Sensor configuration

- Sensor: LIS3DSH, expected WHO_AM_I value 0x3F.
- SPI mode 3, approximately 500 kHz.
- Measurement range: +/-2 g.
- Sensor output data rate: 100 Hz.
- Block data update enabled.
- Application sampling interval: 100 ms.
- Streaming report interval: approximately one second.
- Acceleration is reported in mg: 1000 mg = 1 g.

The application checks for fresh data before reading. Therefore, the number of successful samples can differ from the number of polling intervals.

## LED control

| LED | Function |
|---|---|
| Green LD4, PD12 | TIM4 channel 1 PWM |
| Blue LD6, PD15 | Heartbeat: toggles every 125 ms |
| Red LD5, PD14 | Fatal diagnostic indicator |

Green PWM uses a nominal 1 kHz carrier.

Available modes:

- STEADY: constant configured duty cycle.
- BLINK: 500 ms on, 500 ms off.
- BREATH: one second rising, one second falling.

Brightness sets the duty cycle in STEADY mode and the maximum duty cycle in BLINK and BREATH modes.

After RESET, green starts off at brightness 0 in STEADY mode. Blue starts blinking. LED settings are stored in RAM and are not retained across resets.

## UART commands

Use 115200 baud, eight data bits, no parity, one stop bit, and no flow control.

Commands are uppercase and terminated by LF. CRLF is also accepted.

| Command | Description |
|---|---|
| PING | Return PONG |
| STATUS | Report sensor state, counters, and free RTOS heap |
| SAMPLE | Report the latest sample consumed by the UART task |
| STREAM ON | Enable periodic sample reports |
| STREAM OFF | Disable periodic sample reports |
| BRIGHTNESS 0..100 | Queue a brightness change |
| MODE STEADY | Queue constant brightness mode |
| MODE BLINK | Queue blinking mode |
| MODE BREATH | Queue breathing mode |
| LEDSTATUS | Report LED settings and applied-command count |
| HELP | List commands |

LED changes return OK QUEUED when admitted to the queue. This acknowledges queue admission; LEDSTATUS reports the state published by the LED task after processing commands.

Example responses captured during hardware testing:

    PONG
    LED brightness=50 mode=STEADY output=50 applied=6 rejected=0
    SAMPLE seq=4390 tick=439000 X=1003 Y=59 Z=-235 mg

Invalid brightness arguments return ERR BRIGHTNESS. Unknown commands return ERR COMMAND.

Oversized or invalid input lines return ERR LINE when the terminating newline is received. The line buffer accepts up to 63 command characters.

UART reception errors and ring-buffer drops are counted. This project does not claim lossless reception under unlimited input traffic.

## Build tools

The project was tested on Windows PowerShell with:

- GNU Arm Embedded tools supplied with STM32CubeIDE.
- GNU Make.
- STM32CubeProgrammer CLI.
- Git.

FreeRTOS-Kernel is included as a Git submodule, pinned to V11.3.1:

    3a22924e0a9ddbbc8b0758881c33b3422a5cc20d

The kernel retains its upstream license.

## Get the source

Clone this repository with its submodule:

    git clone --recurse-submodules <repository-url>

For an existing clone:

    git submodule update --init --recursive

## Build and flash

From the project root in PowerShell:

    . .\setup.ps1
    make -j4 --output-sync=target

Flash the Discovery firmware:

    make flash

Build and flash the NUCLEO UART bridge:

    make -C Bridge -j4 --output-sync=target
    make -C Bridge flash

The two Makefiles target different boards using explicit ST-LINK serial numbers. Update these settings for your own boards before flashing.

The root build produces:

    build/stm32_freertos_sensor.elf
    build/stm32_freertos_sensor.bin
    build/stm32_freertos_sensor.hex

Bridge output is stored under:

    build/bridge/

The root clean command removes both sets of build output:

    make clean

## Verified hardware tests

- Initial FreeRTOS LED scheduling and reset behaviour.
- LIS3DSH identity and configuration.
- UART bridge loopback.
- PING, STATUS, and SAMPLE commands over the complete bridge connection.
- Brightness settings: 0%, 10%, 25%, 50%, and 100%.
- Visual BLINK, BREATH, and STEADY behaviour.
- Invalid brightness values, unknown commands, and oversized lines.
- Parser recovery: PING succeeds after an oversized line.
- Invalid commands leave the published LED settings and counters unchanged.
- Streaming sequences increase and STREAM OFF stops periodic output.
- UART remains responsive after streaming.
- Tilting the board changes acceleration readings.
- Green breathing and blue heartbeat continue during sensor streaming.

During the recorded streaming tests, sensor errors, sample drops, UART receive errors, receive drops, and transmit timeouts were all zero. Reported free RTOS heap was 24032 bytes.

These observations describe the tested conditions; they are not a proof of behaviour under every timing or traffic condition.

## Diagnostics and limitations

Assertions, allocation failures, and detected task-stack overflows enter a diagnostic panic handler. Failure information is available through debugger-visible globals.

- SPI transfers and UART transmission use bounded polling.
- UART reception uses an interrupt, a ring buffer, and a binary semaphore.
- The bridge uses interrupt-driven reception and polled transmission.
- No DMA is used in this application.
- No persistent configuration storage is implemented.
- Stack high-water marks and worst-case execution times have not yet been measured.
- Overload behaviour and injected allocation or stack failures have not been validated.
- A zero error count reflects observed testing, not exhaustive fault coverage.

This is a learning and portfolio project.

## References

- FreeRTOS-Kernel: https://github.com/FreeRTOS/FreeRTOS-Kernel
- STM32F407 documentation: https://www.st.com/en/microcontrollers-microprocessors/stm32f407-417/documentation.html
- STM32F407G-DISC1: https://www.st.com/en/evaluation-tools/stm32f407g-disc1.html
- NUCLEO-F439ZI: https://www.st.com/en/evaluation-tools/nucleo-f439zi.html
