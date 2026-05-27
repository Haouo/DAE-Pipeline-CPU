#ifndef DEVICES_DRAM_H
#define DEVICES_DRAM_H

/* DRAM — main memory at 0x8000_0000, 64 MiB locked size for Snake SoC.
 *
 * v0.1 supports only the ideal-latency model (zero-latency byte array).
 * The ctor signature is forward-compatible with Lab 7's abstract DRAM model
 * (bank/row/column + scheduling + timing parameters); selecting
 * DRAM_MODEL_ABSTRACT in v0.1 returns -ENOTSUP.
 *
 * Heap-allocated (64 MiB doesn't fit on the stack), so this device uses
 * the double-pointer ctor/dtor convention.
 *
 * Implements MmioDevice (always) and reserves Tickable for the future
 * abstract model — the Tickable vtbl currently lives in the struct but is
 * not registered in the Ticker when model == IDEAL.
 *
 * Direct access helpers (DRAM_get_backing / DRAM_size_bytes) let the ELF
 * loader bulk-write into DRAM without going through the MmioDevice
 * dispatch loop — necessary for loading megabyte-scale .text/.data
 * efficiently.
 *
 * See plans/snake-soc.md §3.5.6.
 */

#include "common.h"
#include "mmio_device.h"
#include "tickable.h"

#include <stddef.h>
#include <stdint.h>

#define DRAM_DEFAULT_SIZE  (64u * 1024u * 1024u)   /* 64 MiB */

typedef enum {
    DRAM_MODEL_IDEAL    = 0,   /* Lab 4–6: zero-latency byte array */
    DRAM_MODEL_ABSTRACT = 1,   /* Lab 7+: row buffer + scheduling (not in v0.1) */
} dram_model_e;

typedef struct {
    MmioDevice    mmio_super;
    Tickable      tick_super;   /* unused when model == IDEAL */
    dram_model_e  model;
    uint8_t      *backing;      /* heap-allocated; size_bytes long */
    size_t        size_bytes;
} DRAM;

/* Allocates `size_bytes` bytes of zero-initialized backing memory.
 *   Returns  0 on success.
 *   Returns -ENOMEM on allocation failure.
 *   Returns -ENOTSUP if model == DRAM_MODEL_ABSTRACT (Lab 7 deliverable). */
int  DRAM_ctor(DRAM **self, size_t size_bytes, dram_model_e model);

/* Frees the backing memory and nulls the caller's pointer. */
void DRAM_dtor(DRAM **self);

/* Trait accessors (return embedded trait pointer, non-owning view). */
MmioDevice *DRAM_as_mmio    (DRAM *self);
Tickable   *DRAM_as_tickable(DRAM *self);

/* Direct-access helpers used by load_elf and integration tests to bypass
 * the MmioDevice dispatch loop for bulk transfers. The pointer is valid
 * for the lifetime of `self`. */
uint8_t *DRAM_get_backing(DRAM *self);
size_t   DRAM_size_bytes (DRAM *self);

#endif /* DEVICES_DRAM_H */
