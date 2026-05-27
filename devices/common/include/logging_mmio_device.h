#ifndef DEVICES_LOGGING_MMIO_DEVICE_H
#define DEVICES_LOGGING_MMIO_DEVICE_H

/* LoggingMmioDevice — transparent Decorator that traces every read/write
 * to a wrapped MmioDevice.
 *
 * Used by:
 *   - Lab 4 CrossVerify: instrument both ISS-side and RTL-side buses to
 *     produce traces that can be diff'd bit-for-bit.
 *   - Lab 8 perf analysis: extract access patterns from any device window
 *     without touching the underlying device.
 *
 * The decorator IS a MmioDevice — it can be registered in MemoryMap at the
 * same base/size as the wrapped device. read/write delegate to the wrapped
 * device, and emit one trace line per call to the configured sink.
 *
 * Trace format (one line per call):
 *   [tag] R w=W off=HHHHHHHH                 -> v=HHHHHHHH rc=D
 *   [tag] W w=W off=HHHHHHHH val=HHHHHHHH strb=H              rc=D
 *
 * H = lowercase hex (no 0x prefix), D = signed decimal. The decorator does
 * NOT take ownership of the wrapped device or the sink.
 */

#include "common.h"
#include "mmio_device.h"

#include <stdio.h>

typedef struct {
    MmioDevice  super;   /* implements MmioDevice */
    MmioDevice *wrapped; /* non-owning view of the inner device */
    const char *tag;     /* human-readable tag; not copied */
    FILE       *sink;    /* NULL → stderr */
} LoggingMmioDevice;

/* Wrap `wrapped` and emit a trace line per read/write to `sink`.
 *   wrapped — must outlive this LoggingMmioDevice.
 *   tag     — appears in every trace line; not copied (must outlive).
 *   sink    — NULL → stderr; otherwise must outlive this device.
 */
void LoggingMmioDevice_ctor(LoggingMmioDevice *self,
                            MmioDevice        *wrapped,
                            const char        *tag,
                            FILE              *sink);

#endif /* DEVICES_LOGGING_MMIO_DEVICE_H */
