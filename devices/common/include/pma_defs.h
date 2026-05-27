#ifndef DEVICES_PMA_DEFS_H
#define DEVICES_PMA_DEFS_H

/* PMA — Physical Memory Attributes for the Snake SoC.
 *
 * RISC-V Privileged Spec §3.6 conformant minimal implementation: the PMA
 * checker is hardwired from a build-time table (§3.6 explicitly permits
 * this for fixed-config systems). The table lives in specs/pma_table.inc
 * and is shared verbatim with the SV side via a sed transform.
 *
 * Pure C11 + <stdint.h> only — safe to include from both host-side code
 * (devices/, iss/, snake_soc/) and RV32 bare-metal runtime (runtime/).
 *
 * See plans/snake-soc.md §2 for the authoritative PMA table and the
 * cross-layer enforcement contract.
 */

#include <stddef.h>
#include <stdint.h>

/* --- Per-language literal wrapper ---------------------------------------
 * C strips HEX32(N) to 0xN`u`. The SV side defines its own `HEX32 that
 * expands to 32'hN. This is the only piece of preprocessor magic that
 * differs between consumers.
 */
#define HEX32(x) (0x##x##u)

/* --- Enum-like constants ------------------------------------------------
 * Plain #defines so the bare identifiers (PMA_MAIN, PMA_PERM_RW, ...) in
 * pma_table.inc resolve identically on both sides. SV declares these as
 * `localparam`s with matching numeric values.
 */
#define PMA_MAIN      0u
#define PMA_IO        1u

#define PMA_PERM_R    0x1u
#define PMA_PERM_W    0x2u
#define PMA_PERM_X    0x4u
#define PMA_PERM_RW   (PMA_PERM_R | PMA_PERM_W)
#define PMA_PERM_RX   (PMA_PERM_R | PMA_PERM_X)
#define PMA_PERM_RWX  (PMA_PERM_R | PMA_PERM_W | PMA_PERM_X)

#define PMA_CACHE_Y   1u
#define PMA_CACHE_N   0u
#define PMA_IDEMP_Y   1u
#define PMA_IDEMP_N   0u

typedef struct {
    const char *name;
    uint32_t    base;       /* inclusive */
    uint32_t    last;       /* inclusive */
    uint8_t     kind;       /* PMA_MAIN | PMA_IO */
    uint8_t     perms;      /* bitmask of PMA_PERM_* */
    uint8_t     cacheable;
    uint8_t     idemp_r;
    uint8_t     idemp_w;
} pma_entry_t;

/* --- X-macro expansion --------------------------------------------------
 * Define PMA_REGION to a struct-initializer + trailing comma, then pull
 * the table in as the array body. The macro is #undef'd at the end so
 * other translation units may re-include this header in their own
 * X-macro contexts later (e.g., a driver-table generator).
 */
#define PMA_REGION(nm, b, l, k, p, c, ir, iw) \
    { .name = #nm, .base = (b), .last = (l), .kind = (uint8_t)(k), \
      .perms = (uint8_t)(p), .cacheable = (uint8_t)(c), \
      .idemp_r = (uint8_t)(ir), .idemp_w = (uint8_t)(iw) },

/* Anonymous-namespace style: declared `static` so each TU that includes
 * this header gets its own copy. The table is ~8 entries × ~24 bytes so
 * the duplication is harmless and avoids the need for a separate .c file. */
static const pma_entry_t pma_table[] = {
#include "../../../specs/pma_table.inc"
};

#undef PMA_REGION

#define PMA_TABLE_LEN (sizeof(pma_table) / sizeof(pma_table[0]))

/* --- Lookup helpers -----------------------------------------------------
 * Address-range lookup. Linear scan — the table is short enough that a
 * binary search or hash adds more complexity than it saves at runtime.
 * Returns NULL if `addr` falls in an unmapped region (which the AXI
 * fabric reports as DECERR; see plans/snake-soc.md §2.5).
 */
static inline const pma_entry_t *pma_lookup(uint32_t addr) {
    for (size_t i = 0; i < PMA_TABLE_LEN; ++i) {
        if (addr >= pma_table[i].base && addr <= pma_table[i].last) {
            return &pma_table[i];
        }
    }
    return NULL;
}

/* Convenience predicates. Each returns 0 (false) for unmapped addresses
 * — callers that need to distinguish "unmapped" from "mapped but
 * uncacheable" should use pma_lookup() directly and check for NULL. */
static inline int pma_is_cacheable(uint32_t addr) {
    const pma_entry_t *e = pma_lookup(addr);
    return e && e->cacheable;
}

static inline int pma_is_io(uint32_t addr) {
    const pma_entry_t *e = pma_lookup(addr);
    return e && (e->kind == PMA_IO);
}

static inline int pma_perms_allow(uint32_t addr, uint8_t want) {
    const pma_entry_t *e = pma_lookup(addr);
    return e && ((e->perms & want) == want);
}

#endif /* DEVICES_PMA_DEFS_H */
