#ifndef RTOS_HOOKS_H
#define RTOS_HOOKS_H

#include <stdint.h>

#define RTOS_FAILURE_ASSERT     1U
#define RTOS_FAILURE_MALLOC     2U
#define RTOS_FAILURE_STACK      3U
#define RTOS_FAILURE_SCHEDULER  4U

extern volatile uint32_t rtos_failure_code;
extern volatile uint32_t rtos_failure_line;

extern const char * volatile rtos_failure_file;
extern const char * volatile rtos_failure_task;

__attribute__((noreturn))
void rtos_panic(
    uint32_t code,
    const char *file,
    uint32_t line
);

#endif