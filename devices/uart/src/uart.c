/* UART — full-duplex serial peripheral; see uart.h for the contract.
 *
 * Revised 2026-05-25 (TUI pivot): added RX path (STATUS.RX_VALID + RX_DATA
 * + Tickable trait + IRQ-AGG hook). Pre-pivot was TX-only with an 8-byte
 * window. */

#include "uart.h"

#include "common.h"
#include "mmio_device.h"
#include "tickable.h"

#include "irq_agg.h"
#include "terminal_backend.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define UART_TX_DATA 0x0u
#define UART_STATUS  0x4u
#define UART_RX_DATA 0x8u
#define UART_SIZE    0xCu /* 12 B; offsets 0xC..0xFFFFFFFF → SLVERR */

#define STATUS_TX_BUSY_BIT  (1u << 0)
#define STATUS_RX_VALID_BIT (1u << 1)

/* --- MmioDevice trait ------------------------------------------------- */

static DECLARE_MMIO_DEVICE_READ(UART) {
    DEV_ASSERT(self != NULL, "UART::read self NULL");
    UART *self_ = container_of(self, UART, mmio_super);

    if (width != 1 && width != 2 && width != 4) {
        if (value_out != NULL)
            *value_out = 0;
        return AXI_RESP_SLVERR;
    }

    switch (offset) {
    case UART_TX_DATA:
        /* Write-only register. */
        if (value_out != NULL)
            *value_out = 0;
        return AXI_RESP_SLVERR;
    case UART_STATUS:
        /* TX_BUSY is hardwired 0; RX_VALID reflects current RX state. */
        *value_out = self_->rx_valid ? STATUS_RX_VALID_BIT : 0u;
        return AXI_RESP_OKAY;
    case UART_RX_DATA:
        /* PURE PEEK — no side effect, no pop. Read of an empty buffer
         * returns 0. Pop is the explicit W1C write to STATUS bit 1. */
        *value_out = self_->rx_valid ? (uint32_t)self_->rx_byte : 0u;
        return AXI_RESP_OKAY;
    default:
        /* Out-of-range offset within the 12-byte device window. */
        if (value_out != NULL)
            *value_out = 0;
        return AXI_RESP_SLVERR;
    }
}

static DECLARE_MMIO_DEVICE_WRITE(UART) {
    DEV_ASSERT(self != NULL, "UART::write self NULL");
    UART *self_ = container_of(self, UART, mmio_super);

    if (width != 1 && width != 2 && width != 4) {
        return AXI_RESP_SLVERR;
    }
    /* AXI4-Lite minimal subset: width==4 requires strb==0xF. */
    if (width == 4 && strb != 0xF) {
        return AXI_RESP_SLVERR;
    }

    switch (offset) {
    case UART_TX_DATA: {
        FILE *sink = (self_->out != NULL) ? self_->out : stdout;
        if (fputc((int)(value & 0xFFu), sink) == EOF) {
            return AXI_RESP_SLVERR;
        }
        return AXI_RESP_OKAY;
    }
    case UART_STATUS:
        /* W1C bit 1 (RX_VALID) → pop the single-entry RX buffer. Writes
         * to bit 0 (TX_BUSY) and bits 2..31 are silently ignored
         * (snake-soc.md §2.2.3). */
        if (value & STATUS_RX_VALID_BIT) {
            self_->rx_valid = false;
            self_->rx_byte  = 0;
        }
        return AXI_RESP_OKAY;
    case UART_RX_DATA:
        /* Read-only register. */
        return AXI_RESP_SLVERR;
    default:
        /* Out-of-range offset within the 12-byte device window. */
        return AXI_RESP_SLVERR;
    }
}

/* --- Tickable trait ---------------------------------------------------- */

static DECLARE_TICKABLE_TICK(UART) {
    DEV_ASSERT(self != NULL, "UART::tick self NULL");
    UART *self_ = container_of(self, UART, tick_super);

    /* RX path: when the single-entry buffer is empty, attempt a single
     * non-blocking byte read from the TerminalBackend. A successful
     * read latches the byte and raises rx_valid; the rising edge is
     * forwarded to the IRQ aggregator if connected. */
    if (!self_->rx_valid && self_->terminal != NULL) {
        uint8_t byte;
        if (terminal_backend_try_read(self_->terminal, &byte)) {
            self_->rx_byte  = byte;
            self_->rx_valid = true;
            if (self_->irq != NULL) {
                IrqAgg_set_uart_rx_edge(self_->irq, true);
            }
        }
    }
}

/* --- Constructor and accessors ---------------------------------------- */

void UART_ctor(UART *self, FILE *out, TerminalBackend *terminal) {
    DEV_ASSERT(self != NULL, "UART_ctor: self NULL");

    MmioDevice_ctor(&self->mmio_super);
    static struct MmioDeviceVtbl const mmio_vtbl = {
        .read  = &SIGNATURE_MMIO_DEVICE_READ(UART),
        .write = &SIGNATURE_MMIO_DEVICE_WRITE(UART),
    };
    self->mmio_super.vtbl = &mmio_vtbl;

    Tickable_ctor(&self->tick_super);
    static struct TickableVtbl const tick_vtbl = {
        .tick = &SIGNATURE_TICKABLE_TICK(UART),
    };
    self->tick_super.vtbl = &tick_vtbl;

    self->out      = out;      /* NULL → fputc falls back to stdout */
    self->terminal = terminal; /* NULL → RX disabled */
    self->irq      = NULL;     /* connect later via UART_set_irq_agg */
    self->rx_byte  = 0;
    self->rx_valid = false;
}

MmioDevice *UART_as_mmio(UART *self) {
    DEV_ASSERT(self != NULL, "UART_as_mmio: self NULL");
    return &self->mmio_super;
}

Tickable *UART_as_tickable(UART *self) {
    DEV_ASSERT(self != NULL, "UART_as_tickable: self NULL");
    return &self->tick_super;
}

void UART_set_irq_agg(UART *self, IrqAgg *irq) {
    DEV_ASSERT(self != NULL, "UART_set_irq_agg: self NULL");
    self->irq = irq;
}

bool UART_rx_valid(const UART *self) {
    DEV_ASSERT(self != NULL, "UART_rx_valid: self NULL");
    return self->rx_valid;
}

void UART_inject_rx_byte_for_testing(UART *self, uint8_t byte) {
    DEV_ASSERT(self != NULL, "UART_inject_rx_byte_for_testing: self NULL");
    /* Only fire the edge on a fresh rising transition. */
    bool was_valid = self->rx_valid;
    self->rx_byte  = byte;
    self->rx_valid = true;
    if (!was_valid && self->irq != NULL) {
        IrqAgg_set_uart_rx_edge(self->irq, true);
    }
}
