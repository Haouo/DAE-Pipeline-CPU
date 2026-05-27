/* Smoke test for elf_loader: synthesize a minimal in-memory ELF, write it
 * to a tmpfile, point load_elf at it, and verify (a) entry is captured
 * and (b) the segment bytes landed at the right MemoryMap addresses. */

#include "dram.h"
#include "mem_map.h"

#include "axi_resp.h"
#include "elf_loader.h"

#include <assert.h>
#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DRAM_BASE  0x80000000u
#define DRAM_BYTES (4u * 1024u * 1024u) /* 4 MiB — large enough for tests */

/* Build a single-segment ELF on the heap.
 *   - 4 instructions (16 bytes) of payload at p_paddr = 0x80000000
 *   - ELF entry = 0x80000000
 * Returns malloc'd buffer + size via out params; caller frees. */
static void build_minimal_elf(uint8_t **out_buf, size_t *out_size) {
    /* 4 32-bit payload words; the exact contents are arbitrary. */
    static const uint32_t prog[] = {
        0x00500513u, /* addi x10, x0, 5  */
        0xFFF50513u, /* addi x10, x10,-1 */
        0xFE051EE3u, /* bne  x10, x0, -4 */
        0x0000006Fu, /* jal  x0, 0       */
    };
    const size_t prog_bytes = sizeof(prog);

    /* Layout: ehdr | phdr | payload */
    const size_t off_ph      = sizeof(Elf32_Ehdr);
    const size_t off_payload = off_ph + sizeof(Elf32_Phdr);
    const size_t total       = off_payload + prog_bytes;

    uint8_t *buf = (uint8_t *)calloc(total, 1);
    assert(buf != NULL);

    Elf32_Ehdr eh = {0};
    memcpy(eh.e_ident, ELFMAG, SELFMAG);
    eh.e_ident[EI_CLASS]   = ELFCLASS32;
    eh.e_ident[EI_DATA]    = ELFDATA2LSB;
    eh.e_ident[EI_VERSION] = EV_CURRENT;
    eh.e_type              = ET_EXEC;
    eh.e_machine           = EM_RISCV;
    eh.e_version           = EV_CURRENT;
    eh.e_entry             = DRAM_BASE;
    eh.e_phoff             = (Elf32_Off)off_ph;
    eh.e_ehsize            = sizeof(Elf32_Ehdr);
    eh.e_phentsize         = sizeof(Elf32_Phdr);
    eh.e_phnum             = 1;
    memcpy(buf, &eh, sizeof(eh));

    Elf32_Phdr ph = {0};
    ph.p_type     = PT_LOAD;
    ph.p_offset   = (Elf32_Off)off_payload;
    ph.p_vaddr    = DRAM_BASE;
    ph.p_paddr    = DRAM_BASE;
    ph.p_filesz   = (Elf32_Word)prog_bytes;
    ph.p_memsz    = (Elf32_Word)prog_bytes;
    ph.p_flags    = PF_R | PF_X;
    ph.p_align    = 4;
    memcpy(buf + off_ph, &ph, sizeof(ph));

    memcpy(buf + off_payload, prog, prog_bytes);

    *out_buf  = buf;
    *out_size = total;
}

/* Write `buf` to a fresh tmp file and copy the resulting path into `path_out`.
 * `path_out` must point at a writable buffer of at least 64 bytes. Reusing
 * a static buffer here would break across calls — mkstemp consumes the
 * XXXXXX template in place. */
static void write_elf_to_tmp(const uint8_t *buf,
                             size_t         size,
                             char          *path_out,
                             size_t         path_out_size) {
    int n = snprintf(path_out, path_out_size, "/tmp/test_elf_loader.XXXXXX");
    assert(n > 0 && (size_t)n < path_out_size);
    int fd = mkstemp(path_out);
    assert(fd >= 0);
    ssize_t w = write(fd, buf, size);
    assert((size_t)w == size);
    close(fd);
}

static void test_minimal_elf_loads(void) {
    uint8_t *elf_buf = NULL;
    size_t   elf_sz  = 0;
    build_minimal_elf(&elf_buf, &elf_sz);
    char path[64];
    write_elf_to_tmp(elf_buf, elf_sz, path, sizeof(path));

    /* Construct a tiny SoC: just a DRAM device behind a MemoryMap. */
    DRAM *dram = NULL;
    assert(DRAM_ctor(&dram, DRAM_BYTES, DRAM_MODEL_IDEAL) == 0);

    MemoryMap *map = NULL;
    assert(MemoryMap_ctor(&map) == 0);
    mem_map_unit_t unit = {.base = DRAM_BASE, .size = DRAM_BYTES, .device = DRAM_as_mmio(dram)};
    assert(MemoryMap_add_device(map, unit) == 0);

    uint32_t entry = 0;
    int      rc    = load_elf(path, map, &entry);
    assert(rc == 0);
    assert(entry == DRAM_BASE);

    /* Verify the first instruction made it through MemoryMap_write into
     * the DRAM device. Read it back via the same MemoryMap. */
    uint32_t v = 0;
    assert(MemoryMap_read(map, DRAM_BASE, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0x00500513u);

    /* And the last instruction. */
    assert(MemoryMap_read(map, DRAM_BASE + 12, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0x0000006Fu);

    unlink(path);
    MemoryMap_dtor(&map);
    DRAM_dtor(&dram);
    free(elf_buf);

    printf("[OK] test_minimal_elf_loads\n");
}

static void test_bad_magic_rejected(void) {
    uint8_t *elf_buf = NULL;
    size_t   elf_sz  = 0;
    build_minimal_elf(&elf_buf, &elf_sz);
    elf_buf[0] = 0x00; /* corrupt magic */
    char path[64];
    write_elf_to_tmp(elf_buf, elf_sz, path, sizeof(path));

    DRAM *dram = NULL;
    assert(DRAM_ctor(&dram, DRAM_BYTES, DRAM_MODEL_IDEAL) == 0);
    MemoryMap *map = NULL;
    assert(MemoryMap_ctor(&map) == 0);
    mem_map_unit_t unit = {.base = DRAM_BASE, .size = DRAM_BYTES, .device = DRAM_as_mmio(dram)};
    assert(MemoryMap_add_device(map, unit) == 0);

    int rc = load_elf(path, map, NULL);
    assert(rc < 0); /* must reject */

    unlink(path);
    MemoryMap_dtor(&map);
    DRAM_dtor(&dram);
    free(elf_buf);

    printf("[OK] test_bad_magic_rejected\n");
}

int main(void) {
    test_minimal_elf_loads();
    test_bad_magic_rejected();
    printf("All elf_loader tests passed.\n");
    return 0;
}
