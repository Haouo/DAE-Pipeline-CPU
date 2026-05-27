#include "snake_soc_dpi.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "axi_resp.h"
#include "mem_map.h"
#include "snake_soc.h"
#include "ticker.h"
#ifdef __cplusplus
}
#endif

#include <stdint.h>
#include <stdio.h>

static SnakeSoC *g_soc;

static MemoryMap *dpi_map(void) {
    return g_soc ? SnakeSoC_memory_map(g_soc) : NULL;
}

int dpi_snake_soc_init(const char *elf_path) {
    if (g_soc != NULL) {
        SnakeSoC_dtor(&g_soc);
    }

    int rc = SnakeSoC_ctor(&g_soc, stdout);
    if (rc != 0) {
        fprintf(stderr, "[DPI:snake_soc] SnakeSoC_ctor failed: %d\n", rc);
        return rc;
    }

    if (elf_path != NULL && elf_path[0] != '\0') {
        uint32_t entry = 0;
        rc = SnakeSoC_load_elf(g_soc, elf_path, &entry);
        if (rc != 0) {
            fprintf(stderr, "[DPI:snake_soc] load ELF '%s' failed: %d\n", elf_path, rc);
            SnakeSoC_dtor(&g_soc);
            return rc;
        }
        fprintf(stderr, "[DPI:snake_soc] loaded ELF '%s' entry=0x%08x\n", elf_path, entry);
    }

    return 0;
}

void dpi_snake_soc_fini(void) {
    if (g_soc != NULL) {
        SnakeSoC_dtor(&g_soc);
    }
}

void dpi_snake_soc_tick(void) {
    if (g_soc != NULL) {
        Ticker_tick_all(SnakeSoC_ticker(g_soc));
    }
}

int dpi_snake_soc_read(uint32_t addr, int width, uint32_t *value_out) {
    if (value_out == NULL) {
        return AXI_RESP_SLVERR;
    }
    *value_out = 0;
    MemoryMap *map = dpi_map();
    if (map == NULL) {
        return AXI_RESP_DECERR;
    }
    return (int)MemoryMap_read(map, addr, (size_t)width, value_out);
}

int dpi_snake_soc_write(uint32_t addr, int width, uint32_t value, uint8_t strb) {
    MemoryMap *map = dpi_map();
    if (map == NULL) {
        return AXI_RESP_DECERR;
    }
    return (int)MemoryMap_write(map, addr, (size_t)width, value, strb);
}

int dpi_snake_soc_msip(void) {
    return g_soc ? (SnakeSoC_msip_wire(g_soc) ? 1 : 0) : 0;
}

int dpi_snake_soc_mtip(void) {
    return g_soc ? (SnakeSoC_mtip_wire(g_soc) ? 1 : 0) : 0;
}

int dpi_snake_soc_meip(void) {
    return g_soc ? (SnakeSoC_meip_wire(g_soc) ? 1 : 0) : 0;
}
