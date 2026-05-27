/* Model B ELF loader: bulk-copy PT_LOAD segments into a MemoryMap. */

#include "elf_loader.h"

#include "common.h"
#include "mem_map.h"

#include "axi_resp.h"

#include <elf.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Write `len` bytes at `dst` into `target` starting at system address `addr`.
 * Issues 32-bit aligned writes when possible, falling back to byte writes
 * at segment edges. Returns 0 on success or -EFAULT on any non-OKAY
 * MemoryMap response. */
static int write_segment(MemoryMap *target, uint32_t addr, const uint8_t *dst, size_t len) {
    size_t i = 0;

    /* Leading bytes until 4-byte aligned. */
    while (i < len && ((addr + (uint32_t)i) & 0x3u) != 0) {
        axi_resp_e rc = MemoryMap_write(target,
                                        addr + (uint32_t)i,
                                        1,
                                        (uint32_t)dst[i],
                                        (uint8_t)(0x1u << ((addr + i) & 0x3u)));
        if (rc != AXI_RESP_OKAY)
            return -EFAULT;
        i++;
    }

    /* Full 32-bit words. */
    while (i + 4 <= len) {
        uint32_t v = ((uint32_t)dst[i + 0]) | ((uint32_t)dst[i + 1] << 8) |
            ((uint32_t)dst[i + 2] << 16) | ((uint32_t)dst[i + 3] << 24);
        axi_resp_e rc = MemoryMap_write(target, addr + (uint32_t)i, 4, v, 0xF);
        if (rc != AXI_RESP_OKAY)
            return -EFAULT;
        i += 4;
    }

    /* Trailing bytes. */
    while (i < len) {
        axi_resp_e rc = MemoryMap_write(target,
                                        addr + (uint32_t)i,
                                        1,
                                        (uint32_t)dst[i],
                                        (uint8_t)(0x1u << ((addr + i) & 0x3u)));
        if (rc != AXI_RESP_OKAY)
            return -EFAULT;
        i++;
    }

    return 0;
}

int load_elf(const char *path, MemoryMap *target, uint32_t *entry_out) {
    DEV_ASSERT(path != NULL, "load_elf: path NULL");
    DEV_ASSERT(target != NULL, "load_elf: target NULL");

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        DEV_ERR("load_elf: cannot open '%s'", path);
        return -EIO;
    }

    Elf32_Ehdr eh;
    if (fread(&eh, sizeof(eh), 1, f) != 1) {
        DEV_ERR("load_elf: cannot read ELF header");
        fclose(f);
        return -EIO;
    }

    if (memcmp(eh.e_ident, ELFMAG, SELFMAG) != 0) {
        DEV_ERR("load_elf: bad ELF magic");
        fclose(f);
        return -EINVAL;
    }
    if (eh.e_ident[EI_CLASS] != ELFCLASS32) {
        DEV_ERR("load_elf: not ELFCLASS32");
        fclose(f);
        return -EINVAL;
    }
    if (eh.e_machine != EM_RISCV) {
        DEV_ERR("load_elf: not EM_RISCV");
        fclose(f);
        return -EINVAL;
    }
    if (eh.e_type != ET_EXEC) {
        DEV_ERR("load_elf: not ET_EXEC");
        fclose(f);
        return -EINVAL;
    }
    if (eh.e_entry != ELF_LOADER_DRAM_BASE) {
        DEV_ERR("load_elf: entry 0x%08x must equal 0x%08x "
                "(Model B requires entry at DRAM base)",
                (unsigned)eh.e_entry,
                ELF_LOADER_DRAM_BASE);
        fclose(f);
        return -EINVAL;
    }

    /* One reusable segment buffer. PT_LOAD segments are typically small
     * (a few KB to a few MB); stream through this buffer in chunks. */
    enum { CHUNK = 64 * 1024 };
    uint8_t *buf = (uint8_t *)calloc(CHUNK, 1);
    if (buf == NULL) {
        fclose(f);
        return -ENOMEM;
    }

    for (unsigned i = 0; i < eh.e_phnum; i++) {
        if (fseek(f, (long)(eh.e_phoff + i * sizeof(Elf32_Phdr)), SEEK_SET) != 0) {
            DEV_ERR("load_elf: phdr seek failed");
            free(buf);
            fclose(f);
            return -EIO;
        }
        Elf32_Phdr ph;
        if (fread(&ph, sizeof(ph), 1, f) != 1) {
            DEV_ERR("load_elf: phdr read failed");
            free(buf);
            fclose(f);
            return -EIO;
        }

        if (ph.p_type != PT_LOAD || ph.p_filesz == 0)
            continue;

        if (ph.p_paddr != ph.p_vaddr) {
            DEV_ERR("load_elf: segment p_paddr != p_vaddr "
                    "(0x%08x != 0x%08x)",
                    (unsigned)ph.p_paddr,
                    (unsigned)ph.p_vaddr);
            free(buf);
            fclose(f);
            return -EINVAL;
        }

        if (fseek(f, (long)ph.p_offset, SEEK_SET) != 0) {
            DEV_ERR("load_elf: segment seek failed");
            free(buf);
            fclose(f);
            return -EIO;
        }

        uint32_t addr      = ph.p_paddr;
        size_t   remaining = (size_t)ph.p_filesz;
        while (remaining > 0) {
            size_t chunk = remaining > CHUNK ? CHUNK : remaining;
            if (fread(buf, 1, chunk, f) != chunk) {
                DEV_ERR("load_elf: segment read failed");
                free(buf);
                fclose(f);
                return -EIO;
            }
            int wrc = write_segment(target, addr, buf, chunk);
            if (wrc != 0) {
                DEV_ERR("load_elf: write_segment(0x%08x, %zu) -> %d", (unsigned)addr, chunk, wrc);
                free(buf);
                fclose(f);
                return wrc;
            }
            addr += (uint32_t)chunk;
            remaining -= chunk;
        }

        /* Intentionally do NOT zero p_memsz - p_filesz: that is the
         * student's crt0 .bss-clear job per snake-soc.md §5.3. */
    }

    free(buf);
    fclose(f);
    if (entry_out != NULL)
        *entry_out = eh.e_entry;
    return 0;
}
