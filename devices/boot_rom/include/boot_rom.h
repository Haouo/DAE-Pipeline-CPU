#ifndef DEVICES_BOOT_ROM_H
#define DEVICES_BOOT_ROM_H

/* BootROM — 4 KB read-only memory at address 0x0000_0000.
 *
 * Locked layout (plans/snake-soc.md §2.2.1 + §5.2):
 *   0x0000:  0x800000B7   ; lui  x1, 0x80000
 *   0x0004:  0x00008067   ; jalr x0, x1, 0
 *   0x0008-0x0FFF: 0x00
 *
 * After reset the CPU fetches at PC=0, executes the 2 instructions, and
 * jumps unconditionally into DRAM at 0x8000_0000 where the program's
 * _start lives.
 *
 * Implements MmioDevice only. Stack-allocatable (4 KB inline buffer).
 */

#include "common.h"
#include "mmio_device.h"

#include <stdint.h>

#define BOOT_ROM_SIZE    0x1000u     /* 4 KB */
#define BOOT_ROM_STUB_W0 0x800000B7u /* lui  x1, 0x80000 */
#define BOOT_ROM_STUB_W1 0x00008067u /* jalr x0, x1, 0   */

typedef struct {
    MmioDevice super;
    uint8_t    rom[BOOT_ROM_SIZE];
} BootROM;

void BootROM_ctor(BootROM *self);

#endif /* DEVICES_BOOT_ROM_H */
