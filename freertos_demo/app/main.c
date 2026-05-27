/* FreeRTOS-on-ISS demo (post-TUI-pivot 2026-05-25).
 *
 * Three tasks share the CPU:
 *
 *   chatty_a   prio 2, prints "A %u" once per 5 ticks via vTaskDelay
 *   chatty_b   prio 2, prints "B %u" once per 7 ticks via vTaskDelay
 *   uart_rx    prio 3, blocks on a binary semaphore released by the
 *              IRQ-Agg UART-RX-edge handler (port_glue/app_irq_handler.c)
 *
 * Boot sequence:
 *   1. runtime/crt0  → main()
 *   2. install mtvec → freertos_risc_v_trap_handler
 *   3. enable IRQ-Agg UART-RX bit
 *   4. create tasks, create xUartRxSem
 *   5. vTaskStartScheduler() — never returns; if it does, crt0's
 *      tail-call to terminate(0) halts the ISS cleanly via EBREAK.
 *
 * The UART RX path becomes live the moment a byte arrives on the host
 * terminal's stdin (or a piped CI input stream): the UART tick latches
 * the byte into the RX buffer and raises the rising-edge wire, which
 * IRQ-AGG samples into pending[0]; with MEI enabled, the trap handler
 * runs and the ISR gives the semaphore. */

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <runtime.h>
#include <stdint.h>

#define IRQ_AGG_BASE        0x10004000u
#define IRQ_AGG_PENDING     (*(volatile uint32_t *)(IRQ_AGG_BASE + 0x0u))
#define IRQ_AGG_ENABLE      (*(volatile uint32_t *)(IRQ_AGG_BASE + 0x4u))
#define IRQ_BIT_UART_RX     (1u << 0)

/* Visible to port_glue/app_irq_handler.c. */
SemaphoreHandle_t xUartRxSem  = NULL;
volatile uint32_t ulYieldCount = 0;

extern void freertos_risc_v_trap_handler(void);

static void install_mtvec(void) {
    uintptr_t handler = (uintptr_t)&freertos_risc_v_trap_handler;
    /* MODE = 0 (direct).  freertos_risc_v_trap_handler is .align 8 in the
     * port, so the low 2 bits are already 0; assert it anyway. */
    configASSERT((handler & 0x3u) == 0);
    __asm__ volatile("csrw mtvec, %0" : : "r"(handler));
}

static void enable_uart_rx_irq(void) {
    IRQ_AGG_ENABLE  = IRQ_BIT_UART_RX;  /* unmask the source */
    IRQ_AGG_PENDING = IRQ_BIT_UART_RX;  /* W1C any stale latched edge */
}

static void chatty_task(void *pv) {
    const char *tag = (const char *)pv;
    TickType_t period = (tag[0] == 'A') ? pdMS_TO_TICKS(5) : pdMS_TO_TICKS(7);
    uint32_t   tick   = 0;
    for (;;) {
        printf("[%s] tick %u (yields=%u)\n",
               tag, (unsigned)tick++, (unsigned)ulYieldCount);
        vTaskDelay(period);
    }
}

static void uart_rx_task(void *pv) {
    (void)pv;
    uint32_t evt = 0;
    for (;;) {
        if (xSemaphoreTake(xUartRxSem, portMAX_DELAY) == pdTRUE) {
            /* Drain every byte currently in the buffer. The UART exposes
             * a single-entry buffer so at most one byte per ISR firing;
             * loop guards against future FIFO upgrades. */
            int c;
            while ((c = uart_try_getc()) >= 0) {
                printf("[RX] event %u byte=0x%02x\n",
                       (unsigned)evt++, (unsigned)c);
            }
        }
    }
}

int main(void) {
    printf("[boot] main entered\n");
    install_mtvec();
    enable_uart_rx_irq();

    xUartRxSem = xSemaphoreCreateBinary();
    configASSERT(xUartRxSem != NULL);

    BaseType_t rc;
    rc = xTaskCreate(chatty_task, "chatty_A", configMINIMAL_STACK_SIZE,
                     (void *)"A", 2, NULL);
    configASSERT(rc == pdPASS);
    rc = xTaskCreate(chatty_task, "chatty_B", configMINIMAL_STACK_SIZE,
                     (void *)"B", 2, NULL);
    configASSERT(rc == pdPASS);
    rc = xTaskCreate(uart_rx_task, "uart_rx",  configMINIMAL_STACK_SIZE,
                     NULL, 3, NULL);
    configASSERT(rc == pdPASS);

    printf("[boot] scheduler starting\n");
    vTaskStartScheduler();

    /* Only reached if the scheduler bailed (heap exhausted, etc.). */
    printf("[boot] scheduler returned — halting\n");
    return 1;
}
