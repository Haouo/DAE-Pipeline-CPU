#ifndef DEVICES_IRQ_AGG_H
#define DEVICES_IRQ_AGG_H

/* IRQ aggregator — custom minimal interrupt controller (NOT a PLIC).
 *
 * Locked address: 0x1000_4000, size 4 KiB (plans/snake-soc.md §2.2.8).
 * Implements MmioDevice and Tickable.
 *
 * Register map (offsets):
 *   0x0 IRQ_PENDING (R/W1C) — bitmap of edge-latched IRQ sources.
 *                              Write-1-to-clear: software stores 1 in a bit
 *                              to clear that bit; stores of 0 leave bits
 *                              untouched.
 *   0x4 IRQ_ENABLE  (R/W)   — per-source mask. `meip = |(pending & enable)`.
 *
 * Bit assignments (plans/privileged-arch-plan.md §6.2; revised 2026-05-25
 * for the TUI pivot):
 *   bit 0 — UART RX-valid rising edge (driven by UART tick when a fresh
 *           byte arrives in the RX single-entry buffer)
 *   bits 1–31 — reserved, read-only 0
 *
 * Pre-pivot the bit map was: bit 0 Keypad, bit 1 Framebuffer VSYNC, bit 2
 * UART-RX (reserved). After the TUI pivot (Framebuffer + Keypad removed),
 * UART RX is the only main-track source and is promoted to bit 0.
 *
 * Edge sources are sampled in `tick()` from external wires set by the
 * driving devices via `IrqAgg_set_*_edge()`. Each pulse latches the
 * corresponding pending bit; software clears it with W1C.
 *
 * The `meip` wire (queryable via IrqAgg_meip) drives `mip.MEIP[11]` in
 * the CSR file (see plans/privileged-arch-plan.md §3.1). */

#include "common.h"
#include "mmio_device.h"
#include "tickable.h"

#include <stdbool.h>
#include <stdint.h>

#define IRQ_AGG_SIZE 0x1000u /* 4 KiB region; only 8 B used */

#define IRQ_AGG_OFFSET_PENDING 0x0u
#define IRQ_AGG_OFFSET_ENABLE  0x4u

/* Source bit position (only one main-track source post-2026-05-25). */
#define IRQ_AGG_BIT_UART_RX 0

#define IRQ_AGG_VALID_MASK 0x1u /* bit 0 only; bits 1..31 read-only 0 */

typedef struct IrqAgg {
    MmioDevice mmio_super;
    Tickable   tick_super;
    uint32_t   pending;        /* sticky; cleared on W1C; masked to VALID_MASK */
    uint32_t   enable;         /* same masked layout */
    /* Edge input sampled each tick. UART raises this to true for a single
     * tick to latch the pending bit. */
    bool       uart_rx_edge;
} IrqAgg;

/* Stack-allocatable; reset state: pending=0, enable=0, edge=false. */
void IrqAgg_ctor(IrqAgg *self);

MmioDevice *IrqAgg_as_mmio    (IrqAgg *self);
Tickable   *IrqAgg_as_tickable(IrqAgg *self);

/* Wire accessor: meip = |(pending & enable) restricted to known sources. */
bool IrqAgg_meip(const IrqAgg *self);

/* Edge input. UART tick calls this with `true` for one tick after a
 * fresh RX byte has been latched (so the aggregator's own tick — which
 * MUST run after the UART's — observes the rising edge). */
void IrqAgg_set_uart_rx_edge(IrqAgg *self, bool e);

#endif /* DEVICES_IRQ_AGG_H */
