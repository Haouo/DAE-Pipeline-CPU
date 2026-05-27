#include "dram.h"

#include "common.h"
#include "mmio_device.h"
#include "tickable.h"

#include <stdlib.h>
#include <string.h>

static DECLARE_MMIO_DEVICE_READ(DRAM) {
    DEV_ASSERT(self != NULL, "DRAM::read self NULL");
    DRAM *self_ = container_of(self, DRAM, mmio_super);

    if (width != 1 && width != 2 && width != 4) {
        if (value_out != NULL)
            *value_out = 0;
        return AXI_RESP_SLVERR;
    }
    /* Natural-alignment check (RV32I requires aligned access on this bus). */
    if ((offset & (width - 1)) != 0) {
        if (value_out != NULL)
            *value_out = 0;
        return AXI_RESP_SLVERR;
    }
    if (offset + width > self_->size_bytes) {
        if (value_out != NULL)
            *value_out = 0;
        return AXI_RESP_SLVERR;
    }

    uint32_t v = 0;
    for (size_t i = 0; i < width; i++) {
        v |= (uint32_t)self_->backing[offset + i] << (8u * i);
    }
    *value_out = v;
    return AXI_RESP_OKAY;
}

static DECLARE_MMIO_DEVICE_WRITE(DRAM) {
    DEV_ASSERT(self != NULL, "DRAM::write self NULL");
    DRAM *self_ = container_of(self, DRAM, mmio_super);

    if (width != 1 && width != 2 && width != 4) {
        return AXI_RESP_SLVERR;
    }
    if (width == 4 && strb != 0xF) {
        return AXI_RESP_SLVERR;
    }
    if ((offset & (width - 1)) != 0) {
        return AXI_RESP_SLVERR;
    }
    if (offset + width > self_->size_bytes) {
        return AXI_RESP_SLVERR;
    }

    for (size_t i = 0; i < width; i++) {
        if (strb & (uint8_t)(1u << i)) {
            self_->backing[offset + i] = (uint8_t)((value >> (8u * i)) & 0xFFu);
        }
    }
    return AXI_RESP_OKAY;
}

static DECLARE_TICKABLE_TICK(DRAM) {
    DEV_ASSERT(self != NULL, "DRAM::tick self NULL");
    DRAM *self_ = container_of(self, DRAM, tick_super);
    /* IDEAL model has no internal time. ABSTRACT model (Lab 7+) will
     * advance timing counters here. */
    (void)self_;
}

int DRAM_ctor(DRAM **self, size_t size_bytes, dram_model_e model) {
    DEV_ASSERT(self != NULL, "DRAM_ctor: self NULL");
    DEV_ASSERT(size_bytes > 0, "DRAM_ctor: size_bytes must be > 0");

    if (model == DRAM_MODEL_ABSTRACT) {
        /* Lab 7 deliverable; not in v0.1. */
        return -ENOTSUP;
    }

    DRAM *d = calloc(1, sizeof(*d));
    if (d == NULL) {
        return -ENOMEM;
    }
    d->backing = calloc(size_bytes, 1);
    if (d->backing == NULL) {
        free(d);
        return -ENOMEM;
    }

    MmioDevice_ctor(&d->mmio_super);
    static struct MmioDeviceVtbl const mmio_vtbl = {
        .read  = &SIGNATURE_MMIO_DEVICE_READ(DRAM),
        .write = &SIGNATURE_MMIO_DEVICE_WRITE(DRAM),
    };
    d->mmio_super.vtbl = &mmio_vtbl;

    Tickable_ctor(&d->tick_super);
    static struct TickableVtbl const tick_vtbl = {
        .tick = &SIGNATURE_TICKABLE_TICK(DRAM),
    };
    d->tick_super.vtbl = &tick_vtbl;

    d->model      = model;
    d->size_bytes = size_bytes;

    *self = d;
    return 0;
}

void DRAM_dtor(DRAM **self) {
    if (self == NULL || *self == NULL) {
        return;
    }
    free((*self)->backing);
    free(*self);
    *self = NULL;
}

MmioDevice *DRAM_as_mmio(DRAM *self) {
    DEV_ASSERT(self != NULL, "DRAM_as_mmio: self NULL");
    return &self->mmio_super;
}

Tickable *DRAM_as_tickable(DRAM *self) {
    DEV_ASSERT(self != NULL, "DRAM_as_tickable: self NULL");
    return &self->tick_super;
}

uint8_t *DRAM_get_backing(DRAM *self) {
    DEV_ASSERT(self != NULL, "DRAM_get_backing: self NULL");
    return self->backing;
}

size_t DRAM_size_bytes(DRAM *self) {
    DEV_ASSERT(self != NULL, "DRAM_size_bytes: self NULL");
    return self->size_bytes;
}
