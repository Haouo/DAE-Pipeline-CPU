/* External-interrupt handler called by the FreeRTOS RISC-V port whenever
 * mcause indicates an asynchronous interrupt that is neither the machine
 * timer (handled inline by the port) nor an environment call (handled as
 * a synchronous exception, also by the port).
 *
 * Concretely: MEI (cause 0x8000000B from the Snake SoC IRQ aggregator) and
 * MSI (cause 0x80000003 from CLINT.msip[0]) both land here.  We override
 * the .weak default in portASM.S, which spins.
 *
 * The port has already saved the full interrupted context onto the ISR
 * stack before calling us — we may use the standard ABI freely.
 */

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <runtime.h>
#include <stdint.h>

/* Snake SoC peripherals (post-TUI-pivot 2026-05-25). */
#define IRQ_AGG_BASE    0x10004000u
#define IRQ_AGG_PENDING (*(volatile uint32_t *)(IRQ_AGG_BASE + 0x0u))
#define IRQ_AGG_ENABLE  (*(volatile uint32_t *)(IRQ_AGG_BASE + 0x4u))

/* Bit 0 = UART RX-valid edge (only main-track source post-pivot). */
#define IRQ_BIT_UART_RX (1u << 0)

#define CLINT_BASE 0x02000000u
#define CLINT_MSIP (*(volatile uint32_t *)(CLINT_BASE + 0x0u))

#define MCAUSE_INT_BIT  (1u << 31)
#define MCAUSE_CODE_MSI 3u
#define MCAUSE_CODE_MTI 7u
#define MCAUSE_CODE_MEI 11u

extern SemaphoreHandle_t xUartRxSem;
extern volatile uint32_t ulYieldCount;

void freertos_risc_v_application_interrupt_handler(void) {
    uint32_t mcause;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));

    BaseType_t xWoken = pdFALSE;

    if ((mcause & MCAUSE_INT_BIT) == 0) {
        /* Should never land here — synchronous traps go elsewhere. */
        printf("[ISR] spurious sync mcause=0x%08x\n", (unsigned)mcause);
        halt(4);
    }

    switch (mcause & ~MCAUSE_INT_BIT) {
    case MCAUSE_CODE_MEI: {
        uint32_t pending = IRQ_AGG_PENDING;
        if (pending & IRQ_BIT_UART_RX) {
            if (xUartRxSem != NULL) {
                xSemaphoreGiveFromISR(xUartRxSem, &xWoken);
            }
        }
        /* W1C everything we sampled so the level stays low until the
         * next edge. */
        IRQ_AGG_PENDING = pending;
        break;
    }
    case MCAUSE_CODE_MSI: {
        /* Self-IPI path — clear msip[0] so we don't re-enter forever.
         * We don't use vPortYield-via-MSIP in this demo (FreeRTOS yields
         * via ECALL), but the path is wired up for experimentation. */
        CLINT_MSIP = 0;
        ulYieldCount++;
        break;
    }
    default: printf("[ISR] unexpected async mcause=0x%08x\n", (unsigned)mcause); halt(5);
    }

    portYIELD_FROM_ISR(xWoken);
}
