/* CLINT — SiFive-compatible subset. See header for contract. */

#include "clint.h"

#include "axi_resp.h"
#include "common.h"
#include "mmio_device.h"
#include "tickable.h"

#include <stdbool.h>
#include <stdint.h>

#define CLINT_MSIP_MASK 0x1u /* only bit 0 is meaningful in msip[0] */

/* --- Helpers ---------------------------------------------------------- */

static uint32_t mtime_lo(const Clint *self) { return (uint32_t)(self->mtime & 0xFFFFFFFFu); }
static uint32_t mtime_hi(const Clint *self) { return (uint32_t)(self->mtime >> 32); }
static uint32_t mtimecmp_lo(const Clint *self) { return (uint32_t)(self->mtimecmp & 0xFFFFFFFFu); }
static uint32_t mtimecmp_hi(const Clint *self) { return (uint32_t)(self->mtimecmp >> 32); }

static void set_mtime_lo(Clint *self, uint32_t v) {
    self->mtime = (self->mtime & 0xFFFFFFFF00000000ull) | v;
}
static void set_mtime_hi(Clint *self, uint32_t v) {
    self->mtime = (self->mtime & 0x00000000FFFFFFFFull) | ((uint64_t)v << 32);
}
static void set_mtimecmp_lo(Clint *self, uint32_t v) {
    self->mtimecmp = (self->mtimecmp & 0xFFFFFFFF00000000ull) | v;
}
static void set_mtimecmp_hi(Clint *self, uint32_t v) {
    self->mtimecmp = (self->mtimecmp & 0x00000000FFFFFFFFull) | ((uint64_t)v << 32);
}

/* --- MmioDevice trait implementation ---------------------------------- */

static DECLARE_MMIO_DEVICE_READ(Clint) {
    DEV_ASSERT(self != NULL, "Clint::read self NULL");
    Clint *self_ = container_of(self, Clint, mmio_super);

    /* AXI4-Lite minimal subset path: width ∈ {1,2,4}. The Lab 2 ISS path
     * may invoke sub-word reads; for simplicity we serve only width == 4
     * (the spec width for CLINT registers) and reject the rest. */
    if (width != 4 || (offset & 0x3u) != 0) {
        if (value_out != NULL) *value_out = 0;
        return AXI_RESP_SLVERR;
    }

    uint32_t v;
    switch (offset) {
    case CLINT_OFFSET_MSIP:        v = self_->msip & CLINT_MSIP_MASK; break;
    case CLINT_OFFSET_MTIMECMP_LO: v = mtimecmp_lo(self_); break;
    case CLINT_OFFSET_MTIMECMP_HI: v = mtimecmp_hi(self_); break;
    case CLINT_OFFSET_MTIME_LO:    v = mtime_lo(self_); break;
    case CLINT_OFFSET_MTIME_HI:    v = mtime_hi(self_); break;
    default:
        if (value_out != NULL) *value_out = 0;
        return AXI_RESP_SLVERR;
    }
    *value_out = v;
    return AXI_RESP_OKAY;
}

static DECLARE_MMIO_DEVICE_WRITE(Clint) {
    DEV_ASSERT(self != NULL, "Clint::write self NULL");
    Clint *self_ = container_of(self, Clint, mmio_super);

    if (width != 4 || strb != 0xF || (offset & 0x3u) != 0) {
        return AXI_RESP_SLVERR;
    }

    switch (offset) {
    case CLINT_OFFSET_MSIP:
        /* Spec convention: only bit 0 is software-controllable; other
         * bits read-only 0. We mask the write so the architectural state
         * stays clean. */
        self_->msip = value & CLINT_MSIP_MASK;
        return AXI_RESP_OKAY;
    case CLINT_OFFSET_MTIMECMP_LO: set_mtimecmp_lo(self_, value); return AXI_RESP_OKAY;
    case CLINT_OFFSET_MTIMECMP_HI: set_mtimecmp_hi(self_, value); return AXI_RESP_OKAY;
    case CLINT_OFFSET_MTIME_LO:    set_mtime_lo   (self_, value); return AXI_RESP_OKAY;
    case CLINT_OFFSET_MTIME_HI:    set_mtime_hi   (self_, value); return AXI_RESP_OKAY;
    default:
        return AXI_RESP_SLVERR;
    }
}

/* --- Tickable trait implementation ------------------------------------ */

static DECLARE_TICKABLE_TICK(Clint) {
    DEV_ASSERT(self != NULL, "Clint::tick self NULL");
    Clint *self_ = container_of(self, Clint, tick_super);
    /* Instruction-bound tick rate in Lab 2 ISS (plans/privileged-arch-plan.md
     * §5.2). Wraparound is well-defined for uint64_t. */
    self_->mtime += 1u;
}

/* --- Constructor and accessors ---------------------------------------- */

void Clint_ctor(Clint *self) {
    DEV_ASSERT(self != NULL, "Clint_ctor: self NULL");

    MmioDevice_ctor(&self->mmio_super);
    static struct MmioDeviceVtbl const mmio_vtbl = {
        .read  = &SIGNATURE_MMIO_DEVICE_READ(Clint),
        .write = &SIGNATURE_MMIO_DEVICE_WRITE(Clint),
    };
    self->mmio_super.vtbl = &mmio_vtbl;

    Tickable_ctor(&self->tick_super);
    static struct TickableVtbl const tick_vtbl = {
        .tick = &SIGNATURE_TICKABLE_TICK(Clint),
    };
    self->tick_super.vtbl = &tick_vtbl;

    self->msip     = 0;
    self->mtime    = 0;
    self->mtimecmp = 0xFFFFFFFFFFFFFFFFull;
}

MmioDevice *Clint_as_mmio(Clint *self) {
    DEV_ASSERT(self != NULL, "Clint_as_mmio: self NULL");
    return &self->mmio_super;
}

Tickable *Clint_as_tickable(Clint *self) {
    DEV_ASSERT(self != NULL, "Clint_as_tickable: self NULL");
    return &self->tick_super;
}

bool Clint_msip(const Clint *self) {
    DEV_ASSERT(self != NULL, "Clint_msip: self NULL");
    return (self->msip & CLINT_MSIP_MASK) != 0;
}

bool Clint_mtip(const Clint *self) {
    DEV_ASSERT(self != NULL, "Clint_mtip: self NULL");
    return self->mtime >= self->mtimecmp;
}
