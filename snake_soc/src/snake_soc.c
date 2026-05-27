/* SnakeSoC — locked-set device composition + MemoryMap + Ticker glue.
 *
 * Revised 2026-05-25 (TUI pivot): Framebuffer + Keypad + SDL backend
 * removed; UART promoted to full-duplex (TX_DATA + STATUS R/W1C +
 * RX_DATA pure peek), connected to the TerminalBackend singleton for
 * raw-mode stdin and to the IRQ aggregator for RX-valid edge wiring.
 * Net peripheral count: 5 (Boot ROM, CLINT, UART, IRQ-AGG, DRAM).
 *
 * Halt device was removed earlier (2026-05-24); voluntary halt is via
 * EBREAK (handled by the Core, observed via commit_packet.halt_observed). */

#include "snake_soc.h"

#include "boot_rom.h"
#include "clint.h"
#include "dram.h"
#include "irq_agg.h"
#include "terminal_backend.h"
#include "uart.h"

#include "elf_loader.h"

#include "common.h"
#include "mem_map.h"
#include "ticker.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Snake SoC locked address map — see plans/snake-soc.md §2.1
 * (revised 2026-05-25 for the TUI pivot). */
#define ADDR_BOOT_ROM    0x00000000u
#define SIZE_BOOT_ROM    0x00001000u /* 4 KB */

#define ADDR_CLINT       0x02000000u
/* CLINT region size lives as CLINT_SIZE in devices/clint/include/clint.h. */

#define ADDR_UART        0x10001000u
#define SIZE_UART        0x0000000Cu /* 12 B: TX_DATA + STATUS + RX_DATA */

#define ADDR_IRQ_AGG     0x10004000u
/* IRQ_AGG_SIZE lives in devices/irq_agg/include/irq_agg.h. */

#define ADDR_DRAM        0x80000000u

struct SnakeSoC {
    BootROM  boot_rom;
    Clint    clint;
    UART     uart;
    IrqAgg   irq_agg;
    DRAM    *dram; /* heap-allocated; 64 MiB */
    MemoryMap *map;
    Ticker    *ticker;
};

/* --- Helpers ---------------------------------------------------------- */

static int register_unit(MemoryMap *map, addr_t base, size_t size, MmioDevice *dev) {
    mem_map_unit_t unit = {.base = base, .size = size, .device = dev};
    int rc = MemoryMap_add_device(map, unit);
    if (rc != 0) {
        DEV_ERR("MemoryMap_add_device(base=0x%08x, size=0x%zx) failed: %d",
                (unsigned)base, size, rc);
    }
    return rc;
}

/* --- ctor / dtor ------------------------------------------------------ */

int SnakeSoC_ctor(SnakeSoC **self, FILE *uart_out) {
    DEV_ASSERT(self != NULL, "SnakeSoC_ctor: self NULL");

    SnakeSoC *soc = calloc(1, sizeof(*soc));
    if (soc == NULL) return -ENOMEM;

    /* 1. Stack-allocatable devices. The TerminalBackend singleton owns
     * raw-mode stdin; it is initialised lazily inside the UART ctor (via
     * the call below) so test code that constructs a UART with terminal=NULL
     * never installs the termios handlers. */
    BootROM_ctor(&soc->boot_rom);
    Clint_ctor(&soc->clint);
    UART_ctor(&soc->uart, uart_out, terminal_backend_get_singleton());
    IrqAgg_ctor(&soc->irq_agg);

    /* Wire UART RX-valid rising edge into IRQ-AGG bit 0 so software can
     * drive interrupt-driven RX in Lab 5+. */
    UART_set_irq_agg(&soc->uart, &soc->irq_agg);

    /* 2. DRAM (heap-backed, 64 MiB ideal). */
    int rc = DRAM_ctor(&soc->dram, DRAM_DEFAULT_SIZE, DRAM_MODEL_IDEAL);
    if (rc != 0) {
        DEV_ERR("DRAM_ctor failed: %d", rc);
        free(soc);
        return rc;
    }

    /* 3. MemoryMap. */
    rc = MemoryMap_ctor(&soc->map);
    if (rc != 0) {
        DRAM_dtor(&soc->dram);
        free(soc);
        return rc;
    }
    if ((rc = register_unit(soc->map, ADDR_BOOT_ROM, SIZE_BOOT_ROM, &soc->boot_rom.super)) != 0 ||
        (rc = register_unit(soc->map, ADDR_CLINT, CLINT_SIZE, Clint_as_mmio(&soc->clint))) != 0 ||
        (rc = register_unit(soc->map, ADDR_UART, SIZE_UART, UART_as_mmio(&soc->uart))) != 0 ||
        (rc = register_unit(soc->map, ADDR_IRQ_AGG, IRQ_AGG_SIZE,
                            IrqAgg_as_mmio(&soc->irq_agg))) != 0 ||
        (rc = register_unit(soc->map, ADDR_DRAM, DRAM_size_bytes(soc->dram),
                            DRAM_as_mmio(soc->dram))) != 0) {
        MemoryMap_dtor(&soc->map);
        DRAM_dtor(&soc->dram);
        free(soc);
        return rc;
    }

    /* 4. Ticker — peripheral tickables only. The CPU lives in RTL and
     * is clocked by the Verilator testbench, not by this Ticker.
     *
     * Order matters: UART ticks BEFORE IRQ-AGG so the RX-valid edge wire
     * is set before the aggregator samples it. CLINT ticks anywhere
     * (its wires are pure functions of stored state, not edge-driven). */
    rc = Ticker_ctor(&soc->ticker);
    if (rc != 0) {
        MemoryMap_dtor(&soc->map);
        DRAM_dtor(&soc->dram);
        free(soc);
        return rc;
    }
    Ticker_add(soc->ticker, Clint_as_tickable(&soc->clint));
    Ticker_add(soc->ticker, UART_as_tickable(&soc->uart));
    /* IRQ-AGG ticks LAST so it observes the UART RX-valid edge wire set
     * by the UART tick above. */
    Ticker_add(soc->ticker, IrqAgg_as_tickable(&soc->irq_agg));

    *self = soc;
    return 0;
}

void SnakeSoC_dtor(SnakeSoC **self) {
    if (self == NULL || *self == NULL) return;
    SnakeSoC *soc = *self;
    Ticker_dtor(&soc->ticker);
    MemoryMap_dtor(&soc->map);
    DRAM_dtor(&soc->dram);
    free(soc);
    *self = NULL;
}

/* --- Accessors -------------------------------------------------------- */

MemoryMap *SnakeSoC_memory_map(SnakeSoC *self) {
    DEV_ASSERT(self != NULL, "SnakeSoC_memory_map: self NULL");
    return self->map;
}

Ticker *SnakeSoC_ticker(SnakeSoC *self) {
    DEV_ASSERT(self != NULL, "SnakeSoC_ticker: self NULL");
    return self->ticker;
}

bool SnakeSoC_msip_wire(const SnakeSoC *self) {
    DEV_ASSERT(self != NULL, "SnakeSoC_msip_wire: self NULL");
    return Clint_msip(&self->clint);
}

bool SnakeSoC_mtip_wire(const SnakeSoC *self) {
    DEV_ASSERT(self != NULL, "SnakeSoC_mtip_wire: self NULL");
    return Clint_mtip(&self->clint);
}

bool SnakeSoC_meip_wire(const SnakeSoC *self) {
    DEV_ASSERT(self != NULL, "SnakeSoC_meip_wire: self NULL");
    return IrqAgg_meip(&self->irq_agg);
}

int SnakeSoC_load_elf(SnakeSoC *self, const char *path, uint32_t *entry_out) {
    DEV_ASSERT(self != NULL, "SnakeSoC_load_elf: self NULL");
    return load_elf(path, self->map, entry_out);
}
