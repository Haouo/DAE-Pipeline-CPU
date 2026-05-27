/* UART HAL — full-duplex single-byte TX/RX via Snake SoC UART MMIO.
 *
 * Revised 2026-05-25 (TUI pivot): RX path added. STATUS is now R/W1C
 * with bit 1 = RX_VALID; RX_DATA at offset 0x8 is a pure peek (no pop
 * on read). Pop is the explicit W1C write to STATUS bit 1. */
#ifndef RUNTIME_HAL_UART_H
#define RUNTIME_HAL_UART_H

#include <stdint.h>

/* Bit positions inside the UART STATUS register. */
#define UART_STATUS_TX_BUSY  (1u << 0)
#define UART_STATUS_RX_VALID (1u << 1)

/* Send one byte over the UART transmit register.
 *   - Direction: software -> MMIO write at 0x1000_1000.
 *   - Side effect: sim back-end calls putc(c, stdout); STATUS.TX_BUSY stays 0.
 *   - Threading: call from main loop only; not interrupt-safe.
 */
void uart_putc(char c);

/* Read the UART status register; bit 0 is TX_BUSY (always 0 in sim),
 * bit 1 is RX_VALID. */
uint32_t uart_status(void);

/* Block until a byte arrives in the RX buffer, then pop and return it.
 *   - Polling pattern: spin on `uart_status() & UART_STATUS_RX_VALID`,
 *     read `*RX_DATA & 0xFF`, write `UART_STATUS_RX_VALID` to STATUS
 *     (W1C) to pop the buffer.
 *   - Direction: MMIO read at 0x1000_1008 (RX_DATA, pure peek) + MMIO
 *     write at 0x1000_1004 (STATUS, W1C pop).
 *   - Threading: call from main loop only; not interrupt-safe. For
 *     interrupt-driven RX, enable IRQ-AGG bit 0 (UART RX-valid edge)
 *     and `mip.MEIE`, then sleep via WFI.
 */
int uart_getc(void);

/* Non-blocking variant: returns >= 0 (the byte) iff RX_VALID is set,
 * otherwise -1. Pops the buffer on a successful read. */
int uart_try_getc(void);

#endif /* RUNTIME_HAL_UART_H */
