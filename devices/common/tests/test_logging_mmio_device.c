#include "common.h"
#include "logging_mmio_device.h"
#include "mmio_device.h"

#include "axi_resp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal MockMmioDevice for testing the decorator in isolation.
 * Doubles as a worked example of "how to implement the MmioDevice trait." */

typedef struct {
    MmioDevice super;
    /* state observed by tests */
    addr_t   last_read_offset;
    addr_t   last_write_offset;
    uint32_t last_write_value;
    uint8_t  last_write_strb;
    /* canned responses for tests */
    axi_resp_e read_rc;
    axi_resp_e write_rc;
    uint32_t   read_value;
} MockMmioDevice;

static DECLARE_MMIO_DEVICE_READ(MockMmioDevice) {
    MockMmioDevice *self_ = container_of(self, MockMmioDevice, super);
    (void)width;
    self_->last_read_offset = offset;
    *value_out              = self_->read_value;
    return self_->read_rc;
}

static DECLARE_MMIO_DEVICE_WRITE(MockMmioDevice) {
    MockMmioDevice *self_ = container_of(self, MockMmioDevice, super);
    (void)width;
    self_->last_write_offset = offset;
    self_->last_write_value  = value;
    self_->last_write_strb   = strb;
    return self_->write_rc;
}

static void MockMmioDevice_ctor(MockMmioDevice *self) {
    MmioDevice_ctor(&self->super);
    static struct MmioDeviceVtbl const vtbl = {
        .read  = &SIGNATURE_MMIO_DEVICE_READ(MockMmioDevice),
        .write = &SIGNATURE_MMIO_DEVICE_WRITE(MockMmioDevice),
    };
    self->super.vtbl        = &vtbl;
    self->last_read_offset  = 0;
    self->last_write_offset = 0;
    self->last_write_value  = 0;
    self->last_write_strb   = 0;
    self->read_rc           = AXI_RESP_OKAY;
    self->write_rc          = AXI_RESP_OKAY;
    self->read_value        = 0;
}

/* T1: write delegates to wrapped device and emits trace. */
static void test_write_delegates(void) {
    MockMmioDevice mock;
    MockMmioDevice_ctor(&mock);
    FILE *trace = tmpfile();
    assert(trace != NULL);

    LoggingMmioDevice log_dev;
    LoggingMmioDevice_ctor(&log_dev, &mock.super, "mock", trace);

    axi_resp_e rc = MmioDevice_write(&log_dev.super, 0x10, 4, 0xCAFEBABEu, 0xF);
    assert(rc == AXI_RESP_OKAY);
    assert(mock.last_write_offset == 0x10);
    assert(mock.last_write_value == 0xCAFEBABEu);
    assert(mock.last_write_strb == 0xF);

    rewind(trace);
    char buf[256] = {0};
    fread(buf, 1, sizeof(buf) - 1, trace);
    assert(strstr(buf, "[mock]") != NULL);
    assert(strstr(buf, " W ") != NULL);
    assert(strstr(buf, "off=00000010") != NULL);
    assert(strstr(buf, "val=cafebabe") != NULL);
    assert(strstr(buf, "strb=f") != NULL);
    assert(strstr(buf, "rc=OKAY") != NULL);

    fclose(trace);
    printf("[OK] test_write_delegates\n");
}

/* T2: read delegates to wrapped device and emits trace with the read value. */
static void test_read_delegates(void) {
    MockMmioDevice mock;
    MockMmioDevice_ctor(&mock);
    mock.read_value = 0x12345678u;

    FILE             *trace = tmpfile();
    LoggingMmioDevice log_dev;
    LoggingMmioDevice_ctor(&log_dev, &mock.super, "mock", trace);

    uint32_t out = 0;
    assert(MmioDevice_read(&log_dev.super, 0x4, 4, &out) == AXI_RESP_OKAY);
    assert(out == 0x12345678u);
    assert(mock.last_read_offset == 0x4);

    rewind(trace);
    char buf[256] = {0};
    fread(buf, 1, sizeof(buf) - 1, trace);
    assert(strstr(buf, " R ") != NULL);
    assert(strstr(buf, "off=00000004") != NULL);
    assert(strstr(buf, "v=12345678") != NULL);

    fclose(trace);
    printf("[OK] test_read_delegates\n");
}

/* T3: SLVERR from wrapped device is propagated and logged by symbolic name. */
static void test_error_propagation(void) {
    MockMmioDevice mock;
    MockMmioDevice_ctor(&mock);
    mock.write_rc = AXI_RESP_SLVERR;

    FILE             *trace = tmpfile();
    LoggingMmioDevice log_dev;
    LoggingMmioDevice_ctor(&log_dev, &mock.super, "mock", trace);

    axi_resp_e rc = MmioDevice_write(&log_dev.super, 0, 4, 0u, 0xF);
    assert(rc == AXI_RESP_SLVERR);

    rewind(trace);
    char buf[256] = {0};
    fread(buf, 1, sizeof(buf) - 1, trace);
    assert(strstr(buf, "rc=SLVERR") != NULL);

    fclose(trace);
    printf("[OK] test_error_propagation\n");
}

/* T4: NULL sink defaults to stderr without crashing. */
static void test_null_sink_defaults_to_stderr(void) {
    MockMmioDevice mock;
    MockMmioDevice_ctor(&mock);

    LoggingMmioDevice log_dev;
    LoggingMmioDevice_ctor(&log_dev, &mock.super, "mock", NULL);

    /* Sink trace lines go to stderr; we just verify no crash + delegation. */
    assert(MmioDevice_write(&log_dev.super, 0, 4, 0u, 0xF) == AXI_RESP_OKAY);
    assert(mock.last_write_strb == 0xF);

    printf("[OK] test_null_sink_defaults_to_stderr\n");
}

int main(void) {
    test_write_delegates();
    test_read_delegates();
    test_error_propagation();
    test_null_sink_defaults_to_stderr();
    printf("All LoggingMmioDevice tests passed.\n");
    return 0;
}
