#ifndef DEVICES_UART_H
#define DEVICES_UART_H

/* UART — full-duplex serial peripheral.
 *
 * Locked address: 0x1000_1000, size 12 bytes (plans/snake-soc.md §2.2.3,
 * revised 2026-05-25 for the TUI pivot — was 8 B with TX only).
 * Implements MmioDevice + Tickable.
 *
 * Register map (relative to base):
 *   offset 0x0 (TX_DATA, 32 b W): write low byte → fputc(low_byte, out);
 *                                  read returns AXI_RESP_SLVERR (write-only).
 *   offset 0x4 (STATUS, 32 b R/W1C):
 *                                  bit 0 = TX_BUSY (always 0 in sim);
 *                                  bit 1 = RX_VALID (1 = RX_DATA holds a
 *                                          fresh byte);
 *                                  writes: W1C to bit 1 pops the RX buffer
 *                                  (rx_valid → false, rx_byte → 0);
 *                                  writes to bit 0 / bits 2..31 are
 *                                  silently ignored (return OKAY).
 *   offset 0x8 (RX_DATA, 32 b R): low byte = head of the single-entry RX
 *                                  buffer; high bytes read as 0; PURE PEEK
 *                                  (no pop — pop is explicit via STATUS
 *                                  W1C);
 *                                  write returns AXI_RESP_SLVERR (RO).
 *   offset 0xC and beyond: AXI_RESP_SLVERR (out of range; size = 12 B).
 *
 * The single-entry RX buffer is filled by the UART's Tickable.tick
 * callback, which non-blocking-reads from stdin via the TerminalBackend
 * (raw-mode termios, O_NONBLOCK) when the buffer is empty. A rising edge
 * on rx_valid is pushed to the optional IRQ aggregator (set via
 * UART_set_irq_agg) so software can drive interrupt-driven RX in Lab 5+.
 *
 * `out` may be NULL → fputc target falls back to stdout at write time.
 * `terminal` may be NULL → RX path is disabled (tick is a no-op for RX).
 *
 * See plans/snake-soc.md §2.2.3, §3.5.3, §4.
 */

#include "common.h"
#include "mmio_device.h"
#include "tickable.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct IrqAgg          IrqAgg;
typedef struct TerminalBackend TerminalBackend;

typedef struct {
    MmioDevice       mmio_super;
    Tickable         tick_super;
    FILE            *out;
    TerminalBackend *terminal; /* non-owning; may be NULL → RX disabled */
    IrqAgg          *irq;      /* non-owning; may be NULL; set via UART_set_irq_agg */
    uint8_t          rx_byte;
    bool             rx_valid;
} UART;

void UART_ctor(UART *self, FILE *out, TerminalBackend *terminal);

MmioDevice *UART_as_mmio(UART *self);
Tickable   *UART_as_tickable(UART *self);

/* Optionally connect the UART to the IRQ aggregator. A rising edge on
 * rx_valid (caused by a fresh byte arriving in the RX buffer) will
 * push IrqAgg_set_uart_rx_edge(true) so the aggregator's next tick
 * latches the pending bit. Pass NULL to disconnect. Tick order in the
 * Ticker registry must place UART before IRQ-AGG. */
void UART_set_irq_agg(UART *self, IrqAgg *irq);

/* Current RX-valid wire (used by IRQ-AGG sampling and tests). */
bool UART_rx_valid(const UART *self);

/* Test-only helper: inject a byte into the RX buffer as if it had been
 * read from stdin. Bypasses the TerminalBackend. Pushes the rising-edge
 * wire to IRQ-AGG if connected. */
void UART_inject_rx_byte_for_testing(UART *self, uint8_t byte);

#endif /* DEVICES_UART_H */
