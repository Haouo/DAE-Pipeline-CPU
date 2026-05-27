#include "common.h"
#include "dram.h"
#include "mem_map.h"
#include "mmio_device.h"

#include "axi_resp.h"

#include <assert.h>
#include <stdio.h>

#define DRAM_BASE 0x80000000u

static void test_ctor_dtor(void) {
    DRAM *d = NULL;
    /* Use a small size for the test so it runs fast. */
    assert(DRAM_ctor(&d, 64u * 1024u, DRAM_MODEL_IDEAL) == 0);
    assert(d != NULL);
    assert(DRAM_size_bytes(d) == 64u * 1024u);
    assert(DRAM_get_backing(d) != NULL);

    DRAM_dtor(&d);
    assert(d == NULL);

    printf("[OK] test_ctor_dtor\n");
}

static void test_abstract_model_unsupported(void) {
    DRAM *d = NULL;
    /* Ctor uses errno convention — DRAM_ctor is not a bus access. */
    assert(DRAM_ctor(&d, 1024u, DRAM_MODEL_ABSTRACT) == -ENOTSUP);
    assert(d == NULL);

    printf("[OK] test_abstract_model_unsupported\n");
}

static void test_word_read_write(void) {
    DRAM *d = NULL;
    assert(DRAM_ctor(&d, 4096u, DRAM_MODEL_IDEAL) == 0);

    assert(MmioDevice_write(DRAM_as_mmio(d), 0x100, 4, 0xDEADBEEFu, 0xF) == AXI_RESP_OKAY);

    uint32_t v = 0;
    assert(MmioDevice_read(DRAM_as_mmio(d), 0x100, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0xDEADBEEFu);

    DRAM_dtor(&d);
    printf("[OK] test_word_read_write\n");
}

static void test_initial_zero(void) {
    DRAM *d = NULL;
    assert(DRAM_ctor(&d, 4096u, DRAM_MODEL_IDEAL) == 0);

    /* DRAM is zero-initialized (calloc). */
    uint32_t v = 0xABCDu;
    assert(MmioDevice_read(DRAM_as_mmio(d), 0, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0);
    assert(MmioDevice_read(DRAM_as_mmio(d), 0xFFC, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0);

    DRAM_dtor(&d);
    printf("[OK] test_initial_zero\n");
}

static void test_byte_strobe(void) {
    DRAM *d = NULL;
    assert(DRAM_ctor(&d, 4096u, DRAM_MODEL_IDEAL) == 0);

    /* Pre-seed 4 bytes. */
    assert(MmioDevice_write(DRAM_as_mmio(d), 0x10, 4, 0xAABBCCDDu, 0xF) == AXI_RESP_OKAY);

    /* Sub-word write: width=2, offset=0x10 (aligned), strb=0x6 means lanes 1 and 2.
     * With our halfword encoding the device writes (value >> 8*i) into
     * byte (offset + i) when strb bit i is set. For i ∈ {0,1} on this width:
     *   i=0: strb bit 0 NOT set → byte 0x10 unchanged
     *   i=1: strb bit 1 SET     → byte 0x11 = (value >> 8) & 0xFF = 0xEE
     * Lanes 2/3 are not in range for width=2.
     */
    assert(MmioDevice_write(DRAM_as_mmio(d), 0x10, 2, 0x0000EEFFu, 0x6) == AXI_RESP_OKAY);

    uint8_t *back = DRAM_get_backing(d);
    assert(back[0x10] == 0xDD); /* unchanged */
    assert(back[0x11] == 0xEE); /* updated   */
    assert(back[0x12] == 0xBB); /* unchanged (width=2 didn't reach here) */
    assert(back[0x13] == 0xAA); /* unchanged */

    DRAM_dtor(&d);
    printf("[OK] test_byte_strobe\n");
}

/* Out-of-range access within the DRAM window — SLVERR per snake-soc.md §2.5. */
static void test_out_of_range_returns_slverr(void) {
    DRAM *d = NULL;
    assert(DRAM_ctor(&d, 4096u, DRAM_MODEL_IDEAL) == 0);

    assert(MmioDevice_write(DRAM_as_mmio(d), 0x1000, 4, 0u, 0xF) == AXI_RESP_SLVERR);

    uint32_t v = 0xCAFEF00Du; /* sentinel */
    assert(MmioDevice_read(DRAM_as_mmio(d), 0x1000, 4, &v) == AXI_RESP_SLVERR);
    assert(v == 0);

    DRAM_dtor(&d);
    printf("[OK] test_out_of_range_returns_slverr\n");
}

/* width ∉ {1,2,4} → SLVERR. */
static void test_bad_width_returns_slverr(void) {
    DRAM *d = NULL;
    assert(DRAM_ctor(&d, 4096u, DRAM_MODEL_IDEAL) == 0);

    uint32_t v = 0xCAFEF00Du;
    assert(MmioDevice_read(DRAM_as_mmio(d), 0, 3, &v) == AXI_RESP_SLVERR);
    assert(v == 0);
    assert(MmioDevice_read(DRAM_as_mmio(d), 0, 8, &v) == AXI_RESP_SLVERR);
    assert(MmioDevice_write(DRAM_as_mmio(d), 0, 3, 0u, 0xF) == AXI_RESP_SLVERR);

    DRAM_dtor(&d);
    printf("[OK] test_bad_width_returns_slverr\n");
}

/* Misaligned access → SLVERR. */
static void test_misaligned_returns_slverr(void) {
    DRAM *d = NULL;
    assert(DRAM_ctor(&d, 4096u, DRAM_MODEL_IDEAL) == 0);

    uint32_t v = 0;
    assert(MmioDevice_read(DRAM_as_mmio(d), 0x1, 4, &v) == AXI_RESP_SLVERR);
    assert(MmioDevice_read(DRAM_as_mmio(d), 0x3, 2, &v) == AXI_RESP_SLVERR);

    DRAM_dtor(&d);
    printf("[OK] test_misaligned_returns_slverr\n");
}

static void test_direct_backing_access(void) {
    DRAM *d = NULL;
    assert(DRAM_ctor(&d, 4096u, DRAM_MODEL_IDEAL) == 0);

    /* Bulk-write via the backing pointer (load_elf style). */
    uint8_t *back = DRAM_get_backing(d);
    for (size_t i = 0; i < 16; i++) {
        back[i] = (uint8_t)(i + 1);
    }

    /* Read back via the MmioDevice path; should see the same data. */
    uint32_t v = 0;
    assert(MmioDevice_read(DRAM_as_mmio(d), 0, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0x04030201u);

    DRAM_dtor(&d);
    printf("[OK] test_direct_backing_access\n");
}

static void test_memory_map_dispatch(void) {
    DRAM *d = NULL;
    assert(DRAM_ctor(&d, 4096u, DRAM_MODEL_IDEAL) == 0);

    MemoryMap *map = NULL;
    assert(MemoryMap_ctor(&map) == 0);
    mem_map_unit_t unit = {.base = DRAM_BASE, .size = 4096u, .device = DRAM_as_mmio(d)};
    assert(MemoryMap_add_device(map, unit) == 0);

    assert(MemoryMap_write(map, DRAM_BASE + 0x200, 4, 0xCAFEBABEu, 0xF) == AXI_RESP_OKAY);

    uint32_t v = 0;
    assert(MemoryMap_read(map, DRAM_BASE + 0x200, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0xCAFEBABEu);

    MemoryMap_dtor(&map);
    DRAM_dtor(&d);
    printf("[OK] test_memory_map_dispatch\n");
}

int main(void) {
    test_ctor_dtor();
    test_abstract_model_unsupported();
    test_word_read_write();
    test_initial_zero();
    test_byte_strobe();
    test_out_of_range_returns_slverr();
    test_bad_width_returns_slverr();
    test_misaligned_returns_slverr();
    test_direct_backing_access();
    test_memory_map_dispatch();
    printf("All DRAM tests passed.\n");
    return 0;
}
