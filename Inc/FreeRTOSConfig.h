#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* Scheduler and timing: 16 MHz HSI, one tick per millisecond. */
#define configCPU_CLOCK_HZ                      16000000UL
#define configTICK_RATE_HZ                      1000U

#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1

#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128U
#define configMAX_TASK_NAME_LEN                 16

#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_TICKLESS_IDLE                  0

/* Allocate RTOS objects from heap_4.c. */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configTOTAL_HEAP_SIZE                   (32U * 1024U)

#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_TASK_NOTIFICATIONS             1
#define configQUEUE_REGISTRY_SIZE                8

/* Software timers are not needed for the first tasks. */
#define configUSE_TIMERS                        0

/* Diagnostics and failure hooks. */
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK             1

#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0

#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configGENERATE_RUN_TIME_STATS           0

/* APIs used by this project. */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_uxTaskGetStackHighWaterMark      1

/*
 * STM32F407 implements four interrupt-priority bits.
 * Lower numerical values mean higher urgency.
 *
 * An ISR calling FreeRTOS FromISR APIs must use a
 * compatible priority: numerical value 5 or greater here.
 */
#define configPRIO_BITS                         4

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Connect the port handlers to the existing startup vector names. */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

#define configCHECK_HANDLER_INSTALLATION        1

/*
 * Implemented in our application diagnostics module.
 * This declaration does not require FreeRTOS task types.
 */
void rtos_assert_failed(const char *file, uint32_t line);

#define configASSERT(condition)                         \
    do                                                  \
    {                                                   \
        if (!(condition))                               \
        {                                               \
            rtos_assert_failed(__FILE__, __LINE__);      \
        }                                               \
    } while (0)

#endif