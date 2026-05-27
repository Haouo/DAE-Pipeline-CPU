#ifndef ELF_LOADER_H
#define ELF_LOADER_H

/* ELF loader for the Snake SoC (Model B).
 *
 * Parses an ELF binary and writes its loadable segments into the supplied
 * MemoryMap. Routing within the MemoryMap is the dispatcher's job — for a
 * typical RV32 program the loadable segments land at 0x8000_0000 and
 * thus reach the DRAM device.
 *
 * Model B = boot-ROM stub jumps to 0x8000_0000; the program's _start
 * lives in DRAM. All PT_LOAD segments must therefore land in the DRAM
 * window [0x8000_0000, 0x8000_0000 + DRAM_size). The loader checks
 * `e_entry == 0x8000_0000` as a safety net for crt0 / linker mistakes.
 *
 * Per plans/snake-soc.md §5.3 the loader does NOT zero p_memsz beyond
 * p_filesz — that responsibility belongs to the student's crt0.
 *
 * Direct successor to the legacy iss/src/load_elf.c. The signature changed:
 * the destination is now a MemoryMap pointer (so the same loader serves
 * both the Lab 2/3 standalone composition and the Lab 4+ CrossVerify
 * harness — see plans/verification-architecture.md §8). */

#include "mem_map.h"

#include <stddef.h>
#include <stdint.h>

#define ELF_LOADER_DRAM_BASE 0x80000000u

/* Load `path` into `target` via MemoryMap_write.
 *   path        — null-terminated filesystem path; not retained.
 *   target      — MemoryMap that contains a DRAM device covering the ELF
 *                 segments' addresses. Must outlive the call.
 *   entry_out   — on success, set to the ELF entry point (e_entry); may
 *                 be NULL if the caller doesn't need it.
 *
 * Returns 0 on success; negative errno-style code on failure:
 *   -EIO       file open / read failure
 *   -EINVAL    bad ELF (magic / class / machine / type / entry mismatch)
 *   -EFAULT    segment falls outside any device in target, or MemoryMap
 *              returned a non-OKAY response
 *
 * On failure, the MemoryMap may have been partially updated; callers
 * should not rely on its contents. */
int load_elf(const char *path, MemoryMap *target, uint32_t *entry_out);

#endif /* ELF_LOADER_H */
