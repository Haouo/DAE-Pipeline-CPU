/* IRQ aggregator unit tests.
 *
 * Revised 2026-05-25 (TUI pivot): bit 0 is now UART RX-valid edge; the
 * pre-pivot Keypad / Framebuffer-VSYNC / UART-reserved bits are gone.
 */

#include "mmio_device.h"
#include "tickable.h"

#include "axi_resp.h"
#include "irq_agg.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_reset_state(void) {
    IrqAgg a;
    IrqAgg_ctor(&a);

    assert(IrqAgg_meip(&a) == false);
    uint32_t v;
    assert(MmioDevice_read(IrqAgg_as_mmio(&a), IRQ_AGG_OFFSET_PENDING, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0);
    assert(MmioDevice_read(IrqAgg_as_mmio(&a), IRQ_AGG_OFFSET_ENABLE, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0);

    printf("[OK] test_reset_state\n");
}

static void test_edge_latches_pending(void) {
    IrqAgg a;
    IrqAgg_ctor(&a);

    IrqAgg_set_uart_rx_edge(&a, true);
    Tickable_tick(IrqAgg_as_tickable(&a));

    uint32_t v;
    assert(MmioDevice_read(IrqAgg_as_mmio(&a), IRQ_AGG_OFFSET_PENDING, 4, &v) == AXI_RESP_OKAY);
    assert(v == (1u << IRQ_AGG_BIT_UART_RX));

    /* Edge wire is auto-cleared after the tick; another tick doesn't relatch. */
    Tickable_tick(IrqAgg_as_tickable(&a));
    assert(MmioDevice_read(IrqAgg_as_mmio(&a), IRQ_AGG_OFFSET_PENDING, 4, &v) == AXI_RESP_OKAY);
    assert(v == (1u << IRQ_AGG_BIT_UART_RX)); /* still sticky */

    printf("[OK] test_edge_latches_pending\n");
}

static void test_w1c_clears(void) {
    IrqAgg a;
    IrqAgg_ctor(&a);

    IrqAgg_set_uart_rx_edge(&a, true);
    Tickable_tick(IrqAgg_as_tickable(&a));

    uint32_t v;
    assert(MmioDevice_read(IrqAgg_as_mmio(&a), IRQ_AGG_OFFSET_PENDING, 4, &v) == AXI_RESP_OKAY);
    assert(v == (1u << IRQ_AGG_BIT_UART_RX));

    /* Write 1 to bit 0 — pending cleared. */
    assert(MmioDevice_write(IrqAgg_as_mmio(&a),
                            IRQ_AGG_OFFSET_PENDING,
                            4,
                            1u << IRQ_AGG_BIT_UART_RX,
                            0xF) == AXI_RESP_OKAY);
    assert(MmioDevice_read(IrqAgg_as_mmio(&a), IRQ_AGG_OFFSET_PENDING, 4, &v) == AXI_RESP_OKAY);
    assert(v == 0);

    /* Re-latch then write 0 to bit 0: pending unchanged (W1C, not W0C). */
    IrqAgg_set_uart_rx_edge(&a, true);
    Tickable_tick(IrqAgg_as_tickable(&a));
    assert(MmioDevice_write(IrqAgg_as_mmio(&a), IRQ_AGG_OFFSET_PENDING, 4, 0u, 0xF) ==
           AXI_RESP_OKAY);
    assert(MmioDevice_read(IrqAgg_as_mmio(&a), IRQ_AGG_OFFSET_PENDING, 4, &v) == AXI_RESP_OKAY);
    assert(v == (1u << IRQ_AGG_BIT_UART_RX));

    printf("[OK] test_w1c_clears\n");
}

static void test_meip_requires_enable(void) {
    IrqAgg a;
    IrqAgg_ctor(&a);

    /* Latch UART-RX pending; meip stays low because enable=0. */
    IrqAgg_set_uart_rx_edge(&a, true);
    Tickable_tick(IrqAgg_as_tickable(&a));
    assert(IrqAgg_meip(&a) == false);

    /* Enable bit 0. */
    assert(MmioDevice_write(IrqAgg_as_mmio(&a),
                            IRQ_AGG_OFFSET_ENABLE,
                            4,
                            1u << IRQ_AGG_BIT_UART_RX,
                            0xF) == AXI_RESP_OKAY);
    assert(IrqAgg_meip(&a) == true);

    /* Software clears pending → meip drops. */
    assert(MmioDevice_write(IrqAgg_as_mmio(&a),
                            IRQ_AGG_OFFSET_PENDING,
                            4,
                            1u << IRQ_AGG_BIT_UART_RX,
                            0xF) == AXI_RESP_OKAY);
    assert(IrqAgg_meip(&a) == false);

    printf("[OK] test_meip_requires_enable\n");
}

static void test_reserved_bits_masked(void) {
    IrqAgg a;
    IrqAgg_ctor(&a);

    /* Writing 0xFFFFFFFF to ENABLE only stores valid bits (bit 0 only). */
    assert(MmioDevice_write(IrqAgg_as_mmio(&a), IRQ_AGG_OFFSET_ENABLE, 4, 0xFFFFFFFFu, 0xF) ==
           AXI_RESP_OKAY);
    uint32_t v;
    assert(MmioDevice_read(IrqAgg_as_mmio(&a), IRQ_AGG_OFFSET_ENABLE, 4, &v) == AXI_RESP_OKAY);
    assert(v == IRQ_AGG_VALID_MASK);

    printf("[OK] test_reserved_bits_masked\n");
}

int main(void) {
    test_reset_state();
    test_edge_latches_pending();
    test_w1c_clears();
    test_meip_requires_enable();
    test_reserved_bits_masked();
    printf("All IrqAgg tests passed.\n");
    return 0;
}
