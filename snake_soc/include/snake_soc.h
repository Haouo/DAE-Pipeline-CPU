#ifndef SNAKE_SOC_H
#define SNAKE_SOC_H

/* SnakeSoC — Snake-SoC composition used as the C-side backend for the
 * DAE Pipeline CPU's Verilator DPI testbench.
 *
 * Owns the 5 devices (BootROM, CLINT, UART full-duplex, IRQ aggregator,
 * DRAM), the TerminalBackend for stdin RX, the MemoryMap dispatcher,
 * and the Ticker registry. The CPU lives in RTL and drives this SoC
 * through DPI; there is no software Core here.
 *
 * Lifetime:
 *   SnakeSoC_ctor(&soc) allocates and constructs the SoC.
 *   SnakeSoC_dtor(&soc) tears down in reverse-construction order and
 *   sets the caller's pointer to NULL.
 *
 * The MemoryMap and Ticker are non-owning views — the caller must NOT
 * dtor them; their lifetime is tied to the SnakeSoC. */

#include "mem_map.h"
#include "ticker.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct SnakeSoC SnakeSoC;

/* Construct the SoC. Returns 0 on success, -ENOMEM on alloc failure.
 *   uart_out  may be NULL → UART writes go to stdout. */
int  SnakeSoC_ctor(SnakeSoC **self, FILE *uart_out);

/* Destroy. Frees devices in reverse-construction order. Sets *self to NULL. */
void SnakeSoC_dtor(SnakeSoC **self);

/* Non-owning trait views — caller (DPI bridge or RTL testbench) uses
 * these to drive the SoC. */
MemoryMap *SnakeSoC_memory_map(SnakeSoC *self);
Ticker    *SnakeSoC_ticker    (SnakeSoC *self);

/* Sample the three mip wires emitted by the SoC's interrupt-bearing
 * devices (CLINT for MSIP/MTIP, IRQ aggregator for MEIP). The DPI bridge
 * forwards these to the RTL's CSR file so its `mip` view stays
 * consistent. */
bool SnakeSoC_msip_wire(const SnakeSoC *self);
bool SnakeSoC_mtip_wire(const SnakeSoC *self);
bool SnakeSoC_meip_wire(const SnakeSoC *self);

/* Convenience: load an ELF into the SoC's memory map via elf_loader.
 * Returns the same negative errno-style codes as load_elf(); 0 on
 * success. On success, *entry_out (if non-NULL) is set to the ELF
 * entry point. */
int SnakeSoC_load_elf(SnakeSoC *self, const char *path, uint32_t *entry_out);

#endif /* SNAKE_SOC_H */
