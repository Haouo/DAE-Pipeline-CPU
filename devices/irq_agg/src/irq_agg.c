/* IRQ aggregator — custom minimal (NOT PLIC). See header for contract.
 *
 * Revised 2026-05-25 (TUI pivot): the bit-0 source is now UART RX-valid
 * edge; the pre-pivot Keypad / Framebuffer-VSYNC / UART-reserved bits
 * are gone. */

#include "irq_agg.h"

#include "axi_resp.h"
#include "common.h"
#include "mmio_device.h"
#include "tickable.h"

#include <stdbool.h>
#include <stdint.h>

/* --- MmioDevice trait implementation ---------------------------------- */

static DECLARE_MMIO_DEVICE_READ(IrqAgg) {
    DEV_ASSERT(self != NULL, "IrqAgg::read self NULL");
    IrqAgg *self_ = container_of(self, IrqAgg, mmio_super);

    if (width != 4 || (offset & 0x3u) != 0) {
        if (value_out != NULL) *value_out = 0;
        return AXI_RESP_SLVERR;
    }

    switch (offset) {
    case IRQ_AGG_OFFSET_PENDING:
        *value_out = self_->pending & IRQ_AGG_VALID_MASK;
        return AXI_RESP_OKAY;
    case IRQ_AGG_OFFSET_ENABLE:
        *value_out = self_->enable & IRQ_AGG_VALID_MASK;
        return AXI_RESP_OKAY;
    default:
        if (value_out != NULL) *value_out = 0;
        return AXI_RESP_SLVERR;
    }
}

static DECLARE_MMIO_DEVICE_WRITE(IrqAgg) {
    DEV_ASSERT(self != NULL, "IrqAgg::write self NULL");
    IrqAgg *self_ = container_of(self, IrqAgg, mmio_super);

    if (width != 4 || strb != 0xF || (offset & 0x3u) != 0) {
        return AXI_RESP_SLVERR;
    }

    switch (offset) {
    case IRQ_AGG_OFFSET_PENDING:
        /* Write-1-to-clear: clear the bits set in `value`. Bits outside
         * the valid mask are ignored. */
        self_->pending &= ~(value & IRQ_AGG_VALID_MASK);
        return AXI_RESP_OKAY;
    case IRQ_AGG_OFFSET_ENABLE:
        self_->enable = value & IRQ_AGG_VALID_MASK;
        return AXI_RESP_OKAY;
    default:
        return AXI_RESP_SLVERR;
    }
}

/* --- Tickable trait implementation ------------------------------------ */

static DECLARE_TICKABLE_TICK(IrqAgg) {
    DEV_ASSERT(self != NULL, "IrqAgg::tick self NULL");
    IrqAgg *self_ = container_of(self, IrqAgg, tick_super);

    /* Latch the UART-RX edge into pending; auto-clear the edge wire so
     * a single pulse equals a single latched event. The Ticker order
     * (snake_soc.c) guarantees UART ticks before IRQ-AGG so the edge
     * wire reflects this tick's UART state. */
    if (self_->uart_rx_edge) self_->pending |= (1u << IRQ_AGG_BIT_UART_RX);
    self_->uart_rx_edge = false;
}

/* --- Constructor and accessors ---------------------------------------- */

void IrqAgg_ctor(IrqAgg *self) {
    DEV_ASSERT(self != NULL, "IrqAgg_ctor: self NULL");

    MmioDevice_ctor(&self->mmio_super);
    static struct MmioDeviceVtbl const mmio_vtbl = {
        .read  = &SIGNATURE_MMIO_DEVICE_READ(IrqAgg),
        .write = &SIGNATURE_MMIO_DEVICE_WRITE(IrqAgg),
    };
    self->mmio_super.vtbl = &mmio_vtbl;

    Tickable_ctor(&self->tick_super);
    static struct TickableVtbl const tick_vtbl = {
        .tick = &SIGNATURE_TICKABLE_TICK(IrqAgg),
    };
    self->tick_super.vtbl = &tick_vtbl;

    self->pending      = 0;
    self->enable       = 0;
    self->uart_rx_edge = false;
}

MmioDevice *IrqAgg_as_mmio(IrqAgg *self) {
    DEV_ASSERT(self != NULL, "IrqAgg_as_mmio: self NULL");
    return &self->mmio_super;
}

Tickable *IrqAgg_as_tickable(IrqAgg *self) {
    DEV_ASSERT(self != NULL, "IrqAgg_as_tickable: self NULL");
    return &self->tick_super;
}

bool IrqAgg_meip(const IrqAgg *self) {
    DEV_ASSERT(self != NULL, "IrqAgg_meip: self NULL");
    return (self->pending & self->enable & IRQ_AGG_VALID_MASK) != 0;
}

void IrqAgg_set_uart_rx_edge(IrqAgg *self, bool e) {
    DEV_ASSERT(self != NULL, "IrqAgg_set_uart_rx_edge: self NULL");
    self->uart_rx_edge = e;
}
