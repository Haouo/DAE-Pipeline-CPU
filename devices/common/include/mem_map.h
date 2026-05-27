#ifndef DEVICES_MEM_MAP_H
#define DEVICES_MEM_MAP_H

/* MemoryMap — registry of MmioDevice instances dispatched by address range.
 *
 * Non-owning: MemoryMap holds pointers to MmioDevice trait views; the
 * concrete device's lifetime is managed by its owner (typically the ISS
 * struct). MemoryMap_dtor does NOT call any device dtor.
 *
 * Overlap is rejected at registration time (returns -EEXIST), so address
 * decoding errors surface during construction rather than on first access.
 *
 * The dispatcher entry points return `axi_resp_e` per plans/snake-soc.md
 * §2.5: AXI_RESP_DECERR for unmapped addresses (fabric-level), passthrough
 * of the device's AXI_RESP_OKAY / AXI_RESP_SLVERR otherwise. Setup-time
 * APIs (`_ctor`, `_add_device`) keep the errno convention because
 * allocation/registration is not a bus access.
 *
 * See plans/snake-soc.md §3.2.
 */

#include "common.h"
#include "mmio_device.h"

#include "axi_resp.h"

typedef struct {
    addr_t      base;
    size_t      size;
    MmioDevice *device; /* non-owning view */
} mem_map_unit_t;

typedef struct MemoryMap MemoryMap;

/* Construct an empty registry. Allocates from heap.
 *   Returns 0 on success and writes a fresh pointer into *self.
 *   Returns -ENOMEM on allocation failure. */
int MemoryMap_ctor(MemoryMap **self);

/* Destroy the registry. Frees internal storage; does NOT free registered
 * devices. Sets *self to NULL. Safe to call with *self == NULL. */
void MemoryMap_dtor(MemoryMap **self);

/* Register a unit covering [unit.base, unit.base + unit.size).
 *
 *   Returns 0 on success.
 *   Returns -EEXIST if the range overlaps any already-registered unit.
 *   Returns -ENOMEM on internal allocation failure.
 *
 * Order of registration does not affect dispatch. */
int MemoryMap_add_device(MemoryMap *self, mem_map_unit_t unit);

/* Architectural read/write entry points. `addr` is the *system* address;
 * MemoryMap locates the owning device and forwards via MmioDevice_read /
 * MmioDevice_write with `(addr - unit.base)` as the per-device offset.
 *
 *   Returns AXI_RESP_OKAY on success (passthrough from the device).
 *   Returns AXI_RESP_SLVERR if the device returned a subordinate-level
 *   error (e.g., write to a read-only register).
 *   Returns AXI_RESP_DECERR if no device covers [addr, addr + width) —
 *   fabric-level decode error. */
axi_resp_e MemoryMap_read(MemoryMap *self, addr_t addr, size_t width, uint32_t *value_out);
axi_resp_e MemoryMap_write(MemoryMap *self,
                           addr_t     addr,
                           size_t     width,
                           uint32_t   value,
                           uint8_t    strb);

#endif /* DEVICES_MEM_MAP_H */
