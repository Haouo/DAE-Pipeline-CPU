/* UART HAL — TX_DATA at 0x10001000, STATUS at 0x10001004, RX_DATA at
 * 0x10001008. Full-duplex post-2026-05-25 (TUI pivot). */
#include "hal/uart.h"

#include <stdint.h>

#define UART_TX_DATA 0x10001000u
#define UART_STATUS  0x10001004u
#define UART_RX_DATA 0x10001008u

void uart_putc(char c) {
    *(volatile uint32_t *)(uintptr_t)UART_TX_DATA = (uint32_t)(unsigned char)c;
}

uint32_t uart_status(void) {
    return *(volatile uint32_t *)(uintptr_t)UART_STATUS;
}

int uart_getc(void) {
    /* Spin until the device latches a byte. */
    while ((*(volatile uint32_t *)(uintptr_t)UART_STATUS & UART_STATUS_RX_VALID) == 0) {
        /* nothing — caller-side polling. For WFI-driven sleep, enable
         * MEI on the IRQ-AGG UART-RX bit and call __asm__("wfi") here. */
    }
    /* Pure peek — read does not pop. */
    uint8_t byte = (uint8_t)(*(volatile uint32_t *)(uintptr_t)UART_RX_DATA & 0xFFu);
    /* W1C the RX_VALID bit to pop the single-entry buffer. */
    *(volatile uint32_t *)(uintptr_t)UART_STATUS = UART_STATUS_RX_VALID;
    return (int)byte;
}

int uart_try_getc(void) {
    if ((*(volatile uint32_t *)(uintptr_t)UART_STATUS & UART_STATUS_RX_VALID) == 0) {
        return -1;
    }
    uint8_t byte = (uint8_t)(*(volatile uint32_t *)(uintptr_t)UART_RX_DATA & 0xFFu);
    *(volatile uint32_t *)(uintptr_t)UART_STATUS = UART_STATUS_RX_VALID;
    return (int)byte;
}
