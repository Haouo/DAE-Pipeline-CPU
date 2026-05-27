/* CLINT unit tests. */

#include "axi_resp.h"
#include "clint.h"
#include "mem_map.h"
#include "mmio_device.h"
#include "tickable.h"

#include <assert.h>
#include <stdio.h>

#define CLINT_BASE 0x02000000u

static void test_reset_state(void) {
    Clint c;
    Clint_ctor(&c);

    /* msip wire low, mtip wire low (mtime=0 < mtimecmp=UINT64_MAX). */
    assert(Clint_msip(&c) == false);
    assert(Clint_mtip(&c) == false);

    /* Read msip register: should be 0. */
    uint32_t v;
    assert(MmioDevice_read(Clint_as_mmio(&c), CLINT_OFFSET_MSIP, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0);

    /* mtimecmp_lo/hi at reset = 0xFFFFFFFF each. */
    assert(MmioDevice_read(Clint_as_mmio(&c), CLINT_OFFSET_MTIMECMP_LO, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0xFFFFFFFFu);
    assert(MmioDevice_read(Clint_as_mmio(&c), CLINT_OFFSET_MTIMECMP_HI, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0xFFFFFFFFu);

    /* mtime at reset = 0. */
    assert(MmioDevice_read(Clint_as_mmio(&c), CLINT_OFFSET_MTIME_LO, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0);
    assert(MmioDevice_read(Clint_as_mmio(&c), CLINT_OFFSET_MTIME_HI, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0);

    printf("[OK] test_reset_state\n");
}

static void test_msip_writable_bit0(void) {
    Clint c;
    Clint_ctor(&c);

    /* Write 1 → msip bit 0 set; wire goes high. */
    assert(MmioDevice_write(Clint_as_mmio(&c), CLINT_OFFSET_MSIP, 4, 0x1u, 0xF) == AXI_RESP_OKAY);
    assert(Clint_msip(&c) == true);
    uint32_t v;
    assert(MmioDevice_read(Clint_as_mmio(&c), CLINT_OFFSET_MSIP, 4, &v) == AXI_RESP_OKAY);
    assert(v == 1);

    /* Writing 0 clears the bit. */
    assert(MmioDevice_write(Clint_as_mmio(&c), CLINT_OFFSET_MSIP, 4, 0x0u, 0xF) == AXI_RESP_OKAY);
    assert(Clint_msip(&c) == false);

    /* Writing 0xDEADBEEF still only sets bit 0 (mask). */
    assert(MmioDevice_write(Clint_as_mmio(&c), CLINT_OFFSET_MSIP, 4, 0xDEADBEEFu, 0xF) == AXI_RESP_OKAY);
    assert(MmioDevice_read(Clint_as_mmio(&c), CLINT_OFFSET_MSIP, 4, &v) == AXI_RESP_OKAY);
    assert(v == 1); /* bit 0 of 0xDEADBEEF is 1 */
    assert(Clint_msip(&c) == true);

    printf("[OK] test_msip_writable_bit0\n");
}

static void test_tick_advances_mtime(void) {
    Clint c;
    Clint_ctor(&c);

    for (int i = 0; i < 5; i++) {
        Tickable_tick(Clint_as_tickable(&c));
    }
    uint32_t v;
    assert(MmioDevice_read(Clint_as_mmio(&c), CLINT_OFFSET_MTIME_LO, 4, &v) == AXI_RESP_OKAY);
    assert(v == 5);

    printf("[OK] test_tick_advances_mtime\n");
}

static void test_mtip_fires_when_mtime_reaches_mtimecmp(void) {
    Clint c;
    Clint_ctor(&c);

    /* Set mtimecmp_lo = 3, mtimecmp_hi = 0 → mtimecmp = 3. */
    assert(MmioDevice_write(Clint_as_mmio(&c), CLINT_OFFSET_MTIMECMP_LO, 4, 3u, 0xF) == AXI_RESP_OKAY);
    assert(MmioDevice_write(Clint_as_mmio(&c), CLINT_OFFSET_MTIMECMP_HI, 4, 0u, 0xF) == AXI_RESP_OKAY);

    assert(Clint_mtip(&c) == false); /* mtime=0 < 3 */
    Tickable_tick(Clint_as_tickable(&c)); /* mtime=1 */
    assert(Clint_mtip(&c) == false);
    Tickable_tick(Clint_as_tickable(&c)); /* mtime=2 */
    assert(Clint_mtip(&c) == false);
    Tickable_tick(Clint_as_tickable(&c)); /* mtime=3 */
    assert(Clint_mtip(&c) == true);
    Tickable_tick(Clint_as_tickable(&c)); /* mtime=4 — stays high */
    assert(Clint_mtip(&c) == true);

    /* Software clears mtip by writing a larger mtimecmp. */
    assert(MmioDevice_write(Clint_as_mmio(&c), CLINT_OFFSET_MTIMECMP_LO, 4, 100u, 0xF) == AXI_RESP_OKAY);
    assert(Clint_mtip(&c) == false);

    printf("[OK] test_mtip_fires_when_mtime_reaches_mtimecmp\n");
}

static void test_memory_map_dispatch(void) {
    Clint c;
    Clint_ctor(&c);

    MemoryMap *map = NULL;
    assert(MemoryMap_ctor(&map) == 0);
    mem_map_unit_t unit = {.base = CLINT_BASE, .size = CLINT_SIZE, .device = Clint_as_mmio(&c)};
    assert(MemoryMap_add_device(map, unit) == 0);

    /* Round-trip msip via system address. */
    assert(MemoryMap_write(map, CLINT_BASE + CLINT_OFFSET_MSIP, 4, 1u, 0xF) == AXI_RESP_OKAY);
    assert(Clint_msip(&c) == true);

    /* Out-of-region (mid-window unknown offset) → SLVERR. */
    uint32_t v;
    assert(MemoryMap_read(map, CLINT_BASE + 0x100u, 4, &v) == AXI_RESP_SLVERR);

    MemoryMap_dtor(&map);
    printf("[OK] test_memory_map_dispatch\n");
}

static void test_bad_strb_rejected(void) {
    Clint c;
    Clint_ctor(&c);
    /* width=4 with strb != 0xF must be rejected. */
    assert(MmioDevice_write(Clint_as_mmio(&c), CLINT_OFFSET_MSIP, 4, 1u, 0x3) == AXI_RESP_SLVERR);
    assert(Clint_msip(&c) == false);

    printf("[OK] test_bad_strb_rejected\n");
}

int main(void) {
    test_reset_state();
    test_msip_writable_bit0();
    test_tick_advances_mtime();
    test_mtip_fires_when_mtime_reaches_mtimecmp();
    test_memory_map_dispatch();
    test_bad_strb_rejected();
    printf("All CLINT tests passed.\n");
    return 0;
}
