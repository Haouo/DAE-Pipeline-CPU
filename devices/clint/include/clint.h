#ifndef DEVICES_CLINT_H
#define DEVICES_CLINT_H

/* CLINT — Core Local INTerruptor (SiFive-compatible subset).
 *
 * Locked address: 0x0200_0000, size 64 KiB (plans/snake-soc.md §2.2.7).
 * Implements both MmioDevice and Tickable.
 *
 * Register map (offsets):
 *   0x0000  msip[0]        — bit 0 writable; software writes 1 to fire
 *                            MSI self-IPI, 0 to clear. Drives `msip` wire.
 *   0x4000  mtimecmp[0]_lo — low half of 64-bit mtimecmp.
 *   0x4004  mtimecmp[0]_hi — high half.
 *   0xBFF8  mtime_lo       — low half of free-running 64-bit counter.
 *   0xBFFC  mtime_hi       — high half.
 *
 * The Tickable trait advances `mtime` by 1 per call. The C-side standalone
 * runner calls Ticker_tick_all once per ISS step, so `mtime` ticks once
 * per retired instruction (plans/privileged-arch-plan.md §5.2 — Lab 2
 * functional-precision binding).
 *
 * Three wires emerge from the device, queryable via accessors:
 *   - msip = msip[0] bit 0           → drives mip.MSIP[3]
 *   - mtip = (mtime >= mtimecmp)     → drives mip.MTIP[7]
 *   (meip comes from the IRQ aggregator, not CLINT.) */

#include "common.h"
#include "mmio_device.h"
#include "tickable.h"

#include <stdbool.h>
#include <stdint.h>

#define CLINT_SIZE 0x10000u /* 64 KiB region; only a few words live */

#define CLINT_OFFSET_MSIP        0x0000u
#define CLINT_OFFSET_MTIMECMP_LO 0x4000u
#define CLINT_OFFSET_MTIMECMP_HI 0x4004u
#define CLINT_OFFSET_MTIME_LO    0xBFF8u
#define CLINT_OFFSET_MTIME_HI    0xBFFCu

typedef struct {
    MmioDevice mmio_super;
    Tickable   tick_super;
    uint32_t   msip;             /* only bit 0 is meaningful */
    uint64_t   mtime;            /* incremented per tick() */
    uint64_t   mtimecmp;         /* reset 0xFFFF_FFFF_FFFF_FFFF */
} Clint;

/* Stack-allocatable. After ctor:
 *   msip     = 0   (MSI wire low)
 *   mtime    = 0
 *   mtimecmp = 0xFFFF_FFFF_FFFF_FFFF (mtip wire low) */
void Clint_ctor(Clint *self);

/* Trait accessors. */
MmioDevice *Clint_as_mmio    (Clint *self);
Tickable   *Clint_as_tickable(Clint *self);

/* Wire accessors. msip = bit 0 of msip register; mtip = (mtime >= mtimecmp). */
bool Clint_msip(const Clint *self);
bool Clint_mtip(const Clint *self);

#endif /* DEVICES_CLINT_H */
