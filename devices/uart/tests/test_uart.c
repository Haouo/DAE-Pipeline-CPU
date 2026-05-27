/* UART unit tests — full-duplex (post-TUI-pivot 2026-05-25).
 *
 * Tests cover TX (fputc passthrough), STATUS (TX_BUSY hardwired 0,
 * RX_VALID reflects buffer state, W1C to bit 1 pops), RX_DATA (pure
 * peek, never pops on read), invalid offsets and bad strb. RX path uses
 * UART_inject_rx_byte_for_testing to bypass the TerminalBackend so the
 * tests stay deterministic and host-stdin-independent. */

#include "common.h"
#include "mem_map.h"
#include "mmio_device.h"
#include "uart.h"

#include "axi_resp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UART_BASE 0x10001000u
#define UART_SIZE 12u

#define STATUS_TX_BUSY_BIT  (1u << 0)
#define STATUS_RX_VALID_BIT (1u << 1)

/* T1: STATUS reads return 0 at reset (TX_BUSY=0, RX_VALID=0). */
static void test_status_read_reset(void) {
    UART uart;
    UART_ctor(&uart, NULL, NULL);

    uint32_t out = 0xFFFFFFFFu;
    assert(MmioDevice_read(&uart.mmio_super, 0x4, 4, &out) == AXI_RESP_OKAY);
    assert(out == 0);

    printf("[OK] test_status_read_reset\n");
}

/* T1b: TX_DATA is write-only — read returns SLVERR with *value_out = 0. */
static void test_tx_data_read_slverr(void) {
    UART uart;
    UART_ctor(&uart, NULL, NULL);

    uint32_t out = 0xCAFEF00Du;
    assert(MmioDevice_read(&uart.mmio_super, 0x0, 4, &out) == AXI_RESP_SLVERR);
    assert(out == 0);

    printf("[OK] test_tx_data_read_slverr\n");
}

/* T2: TX_DATA writes invoke fputc on a tmpfile; latched value = low byte. */
static void test_tx_data_write_to_tmpfile(void) {
    FILE *fp = tmpfile();
    assert(fp != NULL);

    UART uart;
    UART_ctor(&uart, fp, NULL);

    assert(MmioDevice_write(&uart.mmio_super, 0x0, 4, 0xABCD0041u, 0xF) == AXI_RESP_OKAY); /* 'A' */
    assert(MmioDevice_write(&uart.mmio_super, 0x0, 4, 0x00000042u, 0xF) == AXI_RESP_OKAY); /* 'B' */
    assert(MmioDevice_write(&uart.mmio_super, 0x0, 4, 0x00000043u, 0xF) == AXI_RESP_OKAY); /* 'C' */
    fflush(fp);

    rewind(fp);
    char   buf[8] = {0};
    size_t n      = fread(buf, 1, sizeof(buf) - 1, fp);
    assert(n == 3);
    assert(strcmp(buf, "ABC") == 0);

    fclose(fp);
    printf("[OK] test_tx_data_write_to_tmpfile\n");
}

/* T3: After injecting a byte, STATUS shows RX_VALID and RX_DATA returns it. */
static void test_rx_inject_then_peek(void) {
    UART uart;
    UART_ctor(&uart, NULL, NULL);

    UART_inject_rx_byte_for_testing(&uart, 'Z');

    uint32_t out = 0;
    assert(MmioDevice_read(&uart.mmio_super, 0x4, 4, &out) == AXI_RESP_OKAY);
    assert(out == STATUS_RX_VALID_BIT);

    assert(MmioDevice_read(&uart.mmio_super, 0x8, 4, &out) == AXI_RESP_OKAY);
    assert((out & 0xFFu) == 'Z');

    /* Second read returns the same byte — pure peek, no pop. */
    out = 0;
    assert(MmioDevice_read(&uart.mmio_super, 0x8, 4, &out) == AXI_RESP_OKAY);
    assert((out & 0xFFu) == 'Z');

    printf("[OK] test_rx_inject_then_peek\n");
}

/* T4: W1C to STATUS bit 1 pops the RX buffer. */
static void test_rx_w1c_pops(void) {
    UART uart;
    UART_ctor(&uart, NULL, NULL);

    UART_inject_rx_byte_for_testing(&uart, 'Q');
    assert(UART_rx_valid(&uart) == true);

    /* Write 1 to bit 1 — pops. Bit 0 write is no-op. */
    assert(MmioDevice_write(&uart.mmio_super, 0x4, 4, STATUS_RX_VALID_BIT, 0xF) ==
           AXI_RESP_OKAY);
    assert(UART_rx_valid(&uart) == false);

    uint32_t out = 0xFFu;
    assert(MmioDevice_read(&uart.mmio_super, 0x4, 4, &out) == AXI_RESP_OKAY);
    assert(out == 0);
    assert(MmioDevice_read(&uart.mmio_super, 0x8, 4, &out) == AXI_RESP_OKAY);
    assert(out == 0);

    printf("[OK] test_rx_w1c_pops\n");
}

/* T5: Writes to STATUS bit 0 / bits 2..31 are silently ignored (no SLVERR). */
static void test_status_other_bits_noop(void) {
    UART uart;
    UART_ctor(&uart, NULL, NULL);

    UART_inject_rx_byte_for_testing(&uart, 'X');

    /* Write 1 to bit 0 (TX_BUSY) — no-op; RX still valid. */
    assert(MmioDevice_write(&uart.mmio_super, 0x4, 4, STATUS_TX_BUSY_BIT, 0xF) ==
           AXI_RESP_OKAY);
    assert(UART_rx_valid(&uart) == true);

    /* Write 1 to a reserved bit (bit 31) — no-op; RX still valid. */
    assert(MmioDevice_write(&uart.mmio_super, 0x4, 4, 1u << 31, 0xF) == AXI_RESP_OKAY);
    assert(UART_rx_valid(&uart) == true);

    printf("[OK] test_status_other_bits_noop\n");
}

/* T6: RX_DATA is read-only — write returns SLVERR. */
static void test_rx_data_write_slverr(void) {
    UART uart;
    UART_ctor(&uart, NULL, NULL);

    assert(MmioDevice_write(&uart.mmio_super, 0x8, 4, 0x42u, 0xF) == AXI_RESP_SLVERR);

    printf("[OK] test_rx_data_write_slverr\n");
}

/* T7: Out-of-range offset (>= 0xC, within the 12-byte window) returns SLVERR. */
static void test_out_of_range_slverr(void) {
    UART uart;
    UART_ctor(&uart, NULL, NULL);

    uint32_t out = 0xCAFEu;
    assert(MmioDevice_read(&uart.mmio_super, 0xC, 4, &out) == AXI_RESP_SLVERR);
    assert(out == 0);
    assert(MmioDevice_write(&uart.mmio_super, 0xC, 4, 0u, 0xF) == AXI_RESP_SLVERR);

    printf("[OK] test_out_of_range_slverr\n");
}

/* T8: width=4 with strb != 0xF rejected as SLVERR. */
static void test_bad_strb(void) {
    UART uart;
    UART_ctor(&uart, NULL, NULL);

    assert(MmioDevice_write(&uart.mmio_super, 0x0, 4, 0u, 0x3) == AXI_RESP_SLVERR);

    printf("[OK] test_bad_strb\n");
}

/* T9: Round-trip via MemoryMap dispatcher at the locked base address. */
static void test_memory_map_dispatch(void) {
    FILE *fp = tmpfile();
    assert(fp != NULL);

    UART uart;
    UART_ctor(&uart, fp, NULL);

    MemoryMap *map = NULL;
    assert(MemoryMap_ctor(&map) == 0);

    mem_map_unit_t unit = {.base = UART_BASE, .size = UART_SIZE, .device = &uart.mmio_super};
    assert(MemoryMap_add_device(map, unit) == 0);

    assert(MemoryMap_write(map, UART_BASE + 0x0, 4, 0x5Au, 0xF) == AXI_RESP_OKAY); /* 'Z' */
    fflush(fp);
    rewind(fp);
    char ch;
    assert(fread(&ch, 1, 1, fp) == 1);
    assert(ch == 'Z');

    /* STATUS read at base + 4 */
    uint32_t out = 0;
    assert(MemoryMap_read(map, UART_BASE + 0x4, 4, &out) == AXI_RESP_OKAY);
    assert(out == 0);

    /* RX inject + peek via dispatcher */
    UART_inject_rx_byte_for_testing(&uart, 0x7B); /* '{' */
    assert(MemoryMap_read(map, UART_BASE + 0x8, 4, &out) == AXI_RESP_OKAY);
    assert((out & 0xFFu) == 0x7B);

    MemoryMap_dtor(&map);
    fclose(fp);
    printf("[OK] test_memory_map_dispatch\n");
}

int main(void) {
    test_status_read_reset();
    test_tx_data_read_slverr();
    test_tx_data_write_to_tmpfile();
    test_rx_inject_then_peek();
    test_rx_w1c_pops();
    test_status_other_bits_noop();
    test_rx_data_write_slverr();
    test_out_of_range_slverr();
    test_bad_strb();
    test_memory_map_dispatch();
    printf("All UART tests passed.\n");
    return 0;
}
