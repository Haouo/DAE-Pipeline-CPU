/* Host-only API surface for inspecting the mocked HAL state. */
#ifndef RUNTIME_HOST_TESTS_MOCK_HAL_H
#define RUNTIME_HOST_TESTS_MOCK_HAL_H

#include <stddef.h>

/* Return the captured UART byte stream as a NUL-terminated C string. */
const char *mock_uart_get(void);

/* Number of bytes captured since the last reset. */
size_t mock_uart_len(void);

/* Drop the captured stream and rewind to position 0. */
void mock_uart_reset(void);

/* Configured heap size in bytes — useful for boundary tests. */
size_t mock_heap_size(void);

/* Force the bump allocator back to its empty state. Must be called between
 * tests because malloc/free hold static state. */
void mock_heap_reset(void);

#endif /* RUNTIME_HOST_TESTS_MOCK_HAL_H */
