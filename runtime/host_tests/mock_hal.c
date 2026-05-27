/* Host-side HAL mock — replaces the MMIO-poking implementations in src/hal/
 * with plain in-memory equivalents so the hardware-independent parts of the
 * runtime (string, printf, malloc) can be exercised under host gcc + ctest.
 *
 * The mock captures every uart_putc() call into a fixed-size buffer that the
 * tests inspect via mock_uart_get() / mock_uart_reset(). halt() maps to the
 * host's exit() (so a fatal assertion in tested code aborts the test). The
 * bump allocator's _sheap / _eheap symbols are supplied by a static array
 * (start) and a same-named extern (end), aligned by the linker on most
 * targets via explicit attribute placement.
 */

#include "mock_hal.h"

#include <stdint.h>
#include <stdlib.h>

/* --- UART capture -------------------------------------------------------- */
#define MOCK_UART_CAP 4096
static char   mock_uart_buf[MOCK_UART_CAP];
static size_t mock_uart_pos = 0;

void uart_putc(char c) {
    if (mock_uart_pos < MOCK_UART_CAP - 1) {
        mock_uart_buf[mock_uart_pos++] = c;
    }
}

uint32_t uart_status(void) {
    return 0u;
}

/* RX path mocks: feed bytes via mock_uart_inject_rx_byte (caller-provided
 * test driver). Pop semantics mirror the real HAL: uart_getc blocks
 * spinning on RX_VALID; uart_try_getc returns immediately. Host tests
 * exercise these paths against an in-memory byte queue. */
#define MOCK_RX_CAP 256
static uint8_t mock_rx_buf[MOCK_RX_CAP];
static size_t  mock_rx_head = 0;
static size_t  mock_rx_tail = 0;

int uart_try_getc(void) {
    if (mock_rx_head == mock_rx_tail) return -1;
    uint8_t b = mock_rx_buf[mock_rx_head];
    mock_rx_head = (mock_rx_head + 1) % MOCK_RX_CAP;
    return (int)b;
}

int uart_getc(void) {
    /* Block-equivalent: in tests the queue must be primed; if empty,
     * abort to fail loudly rather than spin forever. */
    int c = uart_try_getc();
    if (c < 0) {
        extern void __attribute__((noreturn)) abort(void);
        abort();
    }
    return c;
}

const char *mock_uart_get(void) {
    mock_uart_buf[mock_uart_pos] = '\0';
    return mock_uart_buf;
}

size_t mock_uart_len(void) {
    return mock_uart_pos;
}

void mock_uart_reset(void) {
    mock_uart_pos = 0;
}

/* --- halt / terminate ---------------------------------------------------- */
__attribute__((noreturn)) void halt(int code) {
    exit(code);
}

__attribute__((noreturn)) void terminate(int code) {
    halt(code);
}

/* --- Heap symbols for the bump allocator ---------------------------------
 *
 * The _sheap / _eheap symbols are defined in mock_heap.S so their addresses
 * are pinned by the assembler (the C compiler is free to reorder C-level
 * data definitions). The bump allocator declares them as `extern char x[]`
 * and treats them as ADDRESS-valued boundaries — never dereferences _eheap.
 */
#define MOCK_HEAP_BYTES (64u * 1024u)

size_t mock_heap_size(void) {
    return MOCK_HEAP_BYTES;
}

/* Reset bump-allocator state by issuing enough free() calls to drive the
 * internal refcount back to zero. Tests must call this between cases.
 * Uses the renamed rt_free symbol directly to avoid the macro indirection. */
extern void rt_free(void *);
void mock_heap_reset(void) {
    for (size_t i = 0; i < MOCK_HEAP_BYTES; i += 4) {
        rt_free((void *)0);
    }
}
