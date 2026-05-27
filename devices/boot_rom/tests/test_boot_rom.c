#include "boot_rom.h"
#include "common.h"
#include "mem_map.h"
#include "mmio_device.h"

#include "axi_resp.h"

#include <assert.h>
#include <stdio.h>

#define BOOT_ROM_BASE 0x00000000u

static void test_stub_layout(void) {
    BootROM rom;
    BootROM_ctor(&rom);

    uint32_t v = 0;
    assert(MmioDevice_read(&rom.super, 0x0, 4, &v) == AXI_RESP_OKAY);
    assert(v == BOOT_ROM_STUB_W0); /* lui  x1, 0x80000 */

    assert(MmioDevice_read(&rom.super, 0x4, 4, &v) == AXI_RESP_OKAY);
    assert(v == BOOT_ROM_STUB_W1); /* jalr x0, x1, 0 */

    /* Padding bytes read as zero. */
    assert(MmioDevice_read(&rom.super, 0x8, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0);

    assert(MmioDevice_read(&rom.super, 0xFFCu, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0);

    printf("[OK] test_stub_layout\n");
}

static void test_writes_rejected(void) {
    BootROM rom;
    BootROM_ctor(&rom);

    assert(MmioDevice_write(&rom.super, 0x0, 4, 0xDEADBEEFu, 0xF) == AXI_RESP_SLVERR);
    /* Verify ROM contents unchanged. */
    uint32_t v = 0;
    assert(MmioDevice_read(&rom.super, 0x0, 4, &v) == AXI_RESP_OKAY);
    assert(v == BOOT_ROM_STUB_W0);

    printf("[OK] test_writes_rejected\n");
}

static void test_byte_read(void) {
    BootROM rom;
    BootROM_ctor(&rom);

    /* Byte-granular reads of the stub. LE order:
     *   word0 = 0x800000B7 → bytes 0xB7 0x00 0x00 0x80 */
    uint32_t v = 0;
    assert(MmioDevice_read(&rom.super, 0x0, 1, &v) == AXI_RESP_OKAY);
    assert(v == 0xB7);
    assert(MmioDevice_read(&rom.super, 0x3, 1, &v) == AXI_RESP_OKAY);
    assert(v == 0x80);

    printf("[OK] test_byte_read\n");
}

/* Out-of-range read within the 4 KiB device window → SLVERR. */
static void test_out_of_range_returns_slverr(void) {
    BootROM rom;
    BootROM_ctor(&rom);

    uint32_t v = 0xCAFEF00Du; /* sentinel */
    assert(MmioDevice_read(&rom.super, BOOT_ROM_SIZE, 4, &v) == AXI_RESP_SLVERR);
    assert(v == 0);

    printf("[OK] test_out_of_range_returns_slverr\n");
}

static void test_memory_map_dispatch(void) {
    BootROM rom;
    BootROM_ctor(&rom);

    MemoryMap *map = NULL;
    assert(MemoryMap_ctor(&map) == 0);

    mem_map_unit_t unit = {.base = BOOT_ROM_BASE, .size = BOOT_ROM_SIZE, .device = &rom.super};
    assert(MemoryMap_add_device(map, unit) == 0);

    /* Reset PC = 0 → fetch the lui instruction. */
    uint32_t v = 0;
    assert(MemoryMap_read(map, BOOT_ROM_BASE + 0x0, 4, &v) == AXI_RESP_OKAY);
    assert(v == BOOT_ROM_STUB_W0);

    /* Next instruction. */
    assert(MemoryMap_read(map, BOOT_ROM_BASE + 0x4, 4, &v) == AXI_RESP_OKAY);
    assert(v == BOOT_ROM_STUB_W1);

    MemoryMap_dtor(&map);
    printf("[OK] test_memory_map_dispatch\n");
}

int main(void) {
    test_stub_layout();
    test_writes_rejected();
    test_byte_read();
    test_out_of_range_returns_slverr();
    test_memory_map_dispatch();
    printf("All BootROM tests passed.\n");
    return 0;
}
