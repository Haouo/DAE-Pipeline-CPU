#include "boot_rom.h"

#include "common.h"
#include "mmio_device.h"

#include <string.h>

/* Little-endian byte order — RV32 is LE. */
static void store_le32(uint8_t *dst, uint32_t value) {
    dst[0] = (uint8_t)((value >> 0) & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static DECLARE_MMIO_DEVICE_READ(BootROM) {
    DEV_ASSERT(self != NULL, "BootROM::read self NULL");
    BootROM *self_ = container_of(self, BootROM, super);

    if (width != 1 && width != 2 && width != 4) {
        if (value_out != NULL)
            *value_out = 0;
        return AXI_RESP_SLVERR;
    }
    /* Out-of-range within the 4 KiB device window — per snake-soc.md §2.5. */
    if (offset + width > BOOT_ROM_SIZE) {
        if (value_out != NULL)
            *value_out = 0;
        return AXI_RESP_SLVERR;
    }

    uint32_t v = 0;
    for (size_t i = 0; i < width; i++) {
        v |= (uint32_t)self_->rom[offset + i] << (8u * i);
    }
    *value_out = v;
    return AXI_RESP_OKAY;
}

static DECLARE_MMIO_DEVICE_WRITE(BootROM) {
    DEV_ASSERT(self != NULL, "BootROM::write self NULL");
    (void)offset;
    (void)width;
    (void)value;
    (void)strb;
    /* Boot ROM is read-only; every write is a subordinate-level error. */
    return AXI_RESP_SLVERR;
}

void BootROM_ctor(BootROM *self) {
    DEV_ASSERT(self != NULL, "BootROM_ctor: self NULL");

    MmioDevice_ctor(&self->super);
    static struct MmioDeviceVtbl const vtbl = {
        .read  = &SIGNATURE_MMIO_DEVICE_READ(BootROM),
        .write = &SIGNATURE_MMIO_DEVICE_WRITE(BootROM),
    };
    self->super.vtbl = &vtbl;

    /* Zero the whole ROM, then patch in the 2-instruction boot stub. */
    memset(self->rom, 0, BOOT_ROM_SIZE);
    store_le32(&self->rom[0x0], BOOT_ROM_STUB_W0);
    store_le32(&self->rom[0x4], BOOT_ROM_STUB_W1);
}
