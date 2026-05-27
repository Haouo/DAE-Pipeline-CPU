/* FreeRTOS hooks for the Snake SoC demo. */

#include "FreeRTOS.h"
#include "task.h"

#include <runtime.h>

void vAssertCalled(const char *file, int line) {
    printf("[FreeRTOS ASSERT] %s:%d — halting\n", file, line);
    taskDISABLE_INTERRUPTS();
    halt(1);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    printf("[FreeRTOS] stack overflow in task '%s' — halting\n", pcTaskName);
    taskDISABLE_INTERRUPTS();
    halt(2);
}

void vApplicationMallocFailedHook(void) {
    printf("[FreeRTOS] pvPortMalloc failed — halting\n");
    taskDISABLE_INTERRUPTS();
    halt(3);
}
