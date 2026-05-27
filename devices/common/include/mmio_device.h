#ifndef DEVICES_MMIO_DEVICE_H
#define DEVICES_MMIO_DEVICE_H

/* MmioDevice trait — Rust-trait-style interface for memory-mapped subordinates.
 *
 * The trait is a struct holding a single member: a pointer to a vtable struct
 * (Rust fat-pointer style; same shape as `AbstractMem` in tmp/ISA-Simulator/).
 * A concrete device embeds `MmioDevice` as a named member — usually `super`
 * (single-trait device) or `mmio_super` (multi-trait device) — and overrides
 * the vtable pointer in its constructor.
 *
 * Trait callbacks recover the concrete-type pointer via `container_of`. See
 * plans/snake-soc.md §3.1.1 for the contract and §3.1.3 for the composition
 * pattern; §2.5 specifies the unified `axi_resp_e` return-code vocabulary
 * shared across the ISS, the AXI4-Lite wire, and the DPI bridge.
 */

#include "common.h"

#include "axi_resp.h"

#include <stddef.h>
#include <stdint.h>

struct MmioDeviceVtbl;

typedef struct {
    struct MmioDeviceVtbl const *vtbl;
} MmioDevice;

struct MmioDeviceVtbl {
    /* Read `width` bytes at byte-offset `offset` (relative to device base).
     *
     *   width ∈ {1, 2, 4}; offset + width <= device size.
     *   On AXI_RESP_OKAY:   *value_out holds the zero-extended value.
     *   On AXI_RESP_SLVERR: subordinate-level error (read of a write-only
     *                       register, malformed transaction, or offset
     *                       outside any defined sub-region); *value_out
     *                       is zeroed.
     *   Subordinates never return AXI_RESP_DECERR — that is fabric-only
     *   (see MemoryMap_*).
     */
    axi_resp_e (*read)(MmioDevice *self, addr_t offset, size_t width, uint32_t *value_out);

    /* Write `value` (low `width` bytes) at `offset` with AXI WSTRB byte-enable.
     *
     *   width ∈ {1, 2, 4}; offset + width <= device size.
     *   strb low nibble is used (bit i enables byte i).
     *   When width == 4, strb MUST equal 0xF (AXI4-Lite minimal subset rule).
     *   Returns AXI_RESP_OKAY on success or AXI_RESP_SLVERR on any
     *   subordinate-level error (write to a read-only register, bad
     *   width/strb, offset outside any defined sub-region).
     */
    axi_resp_e (
        *write)(MmioDevice *self, addr_t offset, size_t width, uint32_t value, uint8_t strb);
};

/* Default constructor — sets vtable to virtual stubs that DEV_PANIC.
 * Concrete devices MUST override `self->vtbl` after calling this. */
void MmioDevice_ctor(MmioDevice *self);

/* Public dispatchers — typically invoked by MemoryMap; route through the
 * vtable so the concrete read/write override runs. */
axi_resp_e MmioDevice_read(MmioDevice *self, addr_t offset, size_t width, uint32_t *value_out);
axi_resp_e MmioDevice_write(MmioDevice *self,
                            addr_t      offset,
                            size_t      width,
                            uint32_t    value,
                            uint8_t     strb);

/* Macros for naming concrete trait-override implementations.
 *
 * Pattern:
 *   DECLARE_MMIO_DEVICE_READ(Halt) { ... }   // defines Halt_MmioDevice_read
 *   ...
 *   static struct MmioDeviceVtbl const vtbl = {
 *       .read  = &SIGNATURE_MMIO_DEVICE_READ (Halt),
 *       .write = &SIGNATURE_MMIO_DEVICE_WRITE(Halt),
 *   };
 *   self->super.vtbl = &vtbl;
 */
#define SIGNATURE_MMIO_DEVICE_READ(cls)  cls##_MmioDevice_read
#define SIGNATURE_MMIO_DEVICE_WRITE(cls) cls##_MmioDevice_write
#define DECLARE_MMIO_DEVICE_READ(cls)      \
    axi_resp_e(SIGNATURE_MMIO_DEVICE_READ( \
        cls))(MmioDevice * self, addr_t offset, size_t width, uint32_t *value_out)
#define DECLARE_MMIO_DEVICE_WRITE(cls)      \
    axi_resp_e(SIGNATURE_MMIO_DEVICE_WRITE( \
        cls))(MmioDevice * self, addr_t offset, size_t width, uint32_t value, uint8_t strb)

#endif /* DEVICES_MMIO_DEVICE_H */
