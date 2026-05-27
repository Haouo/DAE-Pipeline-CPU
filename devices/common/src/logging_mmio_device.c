#include "logging_mmio_device.h"

#include "common.h"
#include "mmio_device.h"

#include <stddef.h>
#include <stdio.h>

static FILE *resolve_sink(FILE *user_sink) {
    return (user_sink != NULL) ? user_sink : stderr;
}

static DECLARE_MMIO_DEVICE_READ(LoggingMmioDevice) {
    DEV_ASSERT(self != NULL, "LoggingMmioDevice::read self NULL");
    LoggingMmioDevice *self_ = container_of(self, LoggingMmioDevice, super);

    uint32_t   v  = 0;
    axi_resp_e rc = MmioDevice_read(self_->wrapped, offset, width, &v);

    fprintf(resolve_sink(self_->sink),
            "[%s] R  w=%zu off=%08x                -> v=%08x rc=%s\n",
            self_->tag,
            width,
            (unsigned)offset,
            v,
            axi_resp_name(rc));

    if (rc == AXI_RESP_OKAY) {
        *value_out = v;
    } else if (value_out != NULL) {
        *value_out = 0;
    }
    return rc;
}

static DECLARE_MMIO_DEVICE_WRITE(LoggingMmioDevice) {
    DEV_ASSERT(self != NULL, "LoggingMmioDevice::write self NULL");
    LoggingMmioDevice *self_ = container_of(self, LoggingMmioDevice, super);

    axi_resp_e rc = MmioDevice_write(self_->wrapped, offset, width, value, strb);

    fprintf(resolve_sink(self_->sink),
            "[%s] W  w=%zu off=%08x val=%08x strb=%x rc=%s\n",
            self_->tag,
            width,
            (unsigned)offset,
            value,
            (unsigned)strb,
            axi_resp_name(rc));

    return rc;
}

void LoggingMmioDevice_ctor(LoggingMmioDevice *self,
                            MmioDevice        *wrapped,
                            const char        *tag,
                            FILE              *sink) {
    DEV_ASSERT(self != NULL, "LoggingMmioDevice_ctor: self NULL");
    DEV_ASSERT(wrapped != NULL, "LoggingMmioDevice_ctor: wrapped NULL");
    DEV_ASSERT(tag != NULL, "LoggingMmioDevice_ctor: tag NULL");

    MmioDevice_ctor(&self->super);
    static struct MmioDeviceVtbl const vtbl = {
        .read  = &SIGNATURE_MMIO_DEVICE_READ(LoggingMmioDevice),
        .write = &SIGNATURE_MMIO_DEVICE_WRITE(LoggingMmioDevice),
    };
    self->super.vtbl = &vtbl;
    self->wrapped    = wrapped;
    self->tag        = tag;
    self->sink       = sink;
}
