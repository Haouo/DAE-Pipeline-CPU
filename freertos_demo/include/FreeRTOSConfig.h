/* FreeRTOSConfig.h — Snake SoC / Lab 2 ISS port.
 *
 * Tick model
 * ----------
 * The Lab 2 ISS advances CLINT `mtime` by exactly one per retired
 * instruction (devices/clint/src/clint.c:95).  So treating "CPU clock"
 * as "instruction count" is exact:
 *   uxTimerIncrementsForOneTick = configCPU_CLOCK_HZ / configTICK_RATE_HZ
 *                               = 1_000_000 / 1_000 = 1000 instructions
 * → an MTI fires every ~1000 retired instructions, which is dense enough
 *   to demonstrate preemption without starving each task.
 *
 * Address map
 * -----------
 * CLINT lives at 0x0200_0000.  The SiFive convention places:
 *   mtime    at +0xBFF8 (64-bit free-running counter)
 *   mtimecmp at +0x4000 (per-hart compare register; we have 1 hart)
 * Matches the Snake SoC spec (CLAUDE.md §7) and the
 * RISCV_MTIME_CLINT_no_extensions chip extension's expectations.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ---- Scheduler ---------------------------------------------------------- */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                ((unsigned short)128)
#define configMAX_TASK_NAME_LEN                 12
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_QUEUE_SETS                    0
#define configUSE_NEWLIB_REENTRANT              0

/* ---- Heap --------------------------------------------------------------- */
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION  0
#define configTOTAL_HEAP_SIZE            ((size_t)(6 * 1024))
#define configAPPLICATION_ALLOCATED_HEAP 0

/* ---- Timers ------------------------------------------------------------- */
#define configUSE_TIMERS             1
#define configTIMER_TASK_PRIORITY    (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH     8
#define configTIMER_TASK_STACK_DEPTH configMINIMAL_STACK_SIZE

/* ---- Hook prototypes (declarations) ------------------------------------ */
#define configASSERT(x)                                   \
    do {                                                  \
        if (!(x)) {                                       \
            extern void vAssertCalled(const char *, int); \
            vAssertCalled(__FILE__, __LINE__);            \
        }                                                 \
    } while (0)

/* ---- RISC-V specifics --------------------------------------------------- */
#define configCPU_CLOCK_HZ ((unsigned long)1000000)
#define configTICK_RATE_HZ ((TickType_t)1000)

/* Snake SoC CLINT — see CLAUDE.md §7. */
#define configMTIME_BASE_ADDRESS    (0x0200BFF8UL)
#define configMTIMECMP_BASE_ADDRESS (0x02004000UL)

/* Have port.c allocate a dedicated ISR stack array (StackType_t[N]) and
 * its top-of-stack symbol xISRStackTop.  Avoids needing a linker symbol
 * (__freertos_irq_stack_top) carved out of the link script. */
#define configISR_STACK_SIZE_WORDS ((size_t)128)

/* ---- Optional features off --------------------------------------------- */
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_vTaskPrioritySet            0
#define INCLUDE_uxTaskPriorityGet           0
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_xSemaphoreGetMutexHolder    0
#define INCLUDE_uxTaskGetStackHighWaterMark 0

#endif /* FREERTOS_CONFIG_H */
