#include "mmio_device.h"

#include "common.h"

#include "axi_resp.h"

/* Compile-time ABI sanity — catch struct-layout drift before runtime.
 * If anyone adds a data member to MmioDevice (instead of factoring it
 * into a separate trait), the build breaks here. */
_Static_assert(sizeof(MmioDevice) == sizeof(struct MmioDeviceVtbl const *),
               "MmioDevice must be exactly one vtable pointer; "
               "do not add data members to the trait struct");

/* Default vtable methods that fire if a concrete subclass forgot to
 * override the vtable pointer. The DEV_PANIC ensures the bug surfaces
 * loudly at the first access rather than silently doing nothing. */

static axi_resp_e default_read(MmioDevice *self, addr_t offset, size_t width, uint32_t *value_out) {
    (void)self;
    (void)offset;
    (void)width;
    (void)value_out;
    DEV_PANIC("MmioDevice::read called on a device whose vtable was not "
              "overridden — concrete ctor must set self->vtbl");
    return AXI_RESP_SLVERR;
}

static axi_resp_e default_write(MmioDevice *self,
                                addr_t      offset,
                                size_t      width,
                                uint32_t    value,
                                uint8_t     strb) {
    (void)self;
    (void)offset;
    (void)width;
    (void)value;
    (void)strb;
    DEV_PANIC("MmioDevice::write called on a device whose vtable was not "
              "overridden — concrete ctor must set self->vtbl");
    return AXI_RESP_SLVERR;
}

void MmioDevice_ctor(MmioDevice *self) {
    DEV_ASSERT(self != NULL, "MmioDevice_ctor: self must not be NULL");
    static struct MmioDeviceVtbl const default_vtbl = {
        .read  = &default_read,
        .write = &default_write,
    };
    self->vtbl = &default_vtbl;
}

axi_resp_e MmioDevice_read(MmioDevice *self, addr_t offset, size_t width, uint32_t *value_out) {
    DEV_ASSERT(self != NULL && self->vtbl != NULL, "MmioDevice_read: self or vtbl is NULL");
    return self->vtbl->read(self, offset, width, value_out);
}

axi_resp_e MmioDevice_write(MmioDevice *self,
                            addr_t      offset,
                            size_t      width,
                            uint32_t    value,
                            uint8_t     strb) {
    DEV_ASSERT(self != NULL && self->vtbl != NULL, "MmioDevice_write: self or vtbl is NULL");
    return self->vtbl->write(self, offset, width, value, strb);
}
