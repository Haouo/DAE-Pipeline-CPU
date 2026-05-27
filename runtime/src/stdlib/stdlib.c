/* Bump allocator + exit().
 *
 * The heap region [_sheap, _eheap) is provided by the linker script on the
 * RV target and by mock_hal.c on the host. The allocator is otherwise pure C
 * with no hardware dependency, so host tests link this object as-is.
 *
 * free() decrements a per-object refcount; the bump pointer resets only when
 * every allocation has been released, matching the legacy Base-Runtime
 * semantics. The (size + 3) & ~3u alignment formula fixes the Base-Runtime
 * bug where `(size + 3) % 4` happily produced a 1-3 byte bump.
 *
 * NOTE: extern char x[] (not extern char *x) is required — these are LINKER
 * SYMBOLS whose ADDRESS is the boundary, not pointers stored in memory.
 */
#include "stdlib/stdlib.h"

#include "hal/halt.h"

#include <stddef.h>
#include <stdint.h>

extern char _sheap[];
extern char _eheap[];

static char    *heap_ptr      = (char *)0;
static unsigned allocated_cnt = 0u;

/* Lazy init so callers don't need to invoke an explicit setup routine.
 * Safe because the runtime is single-threaded (CLAUDE.md §8). */
static void heap_init_if_needed(void) {
    if (heap_ptr == (char *)0) {
        heap_ptr = _sheap;
    }
}

void *malloc(size_t size) {
    heap_init_if_needed();

    /* Align UP to a 4-byte boundary. The legacy `(size + 3) % 4` only
     * produced the low 2 bits of the rounded size; `& ~3u` is the correct
     * round-up-to-multiple-of-4 form. */
    size_t aligned = (size + 3u) & ~(size_t)3u;

    if (heap_ptr + aligned > _eheap) {
        return (void *)0;
    }

    void *p = (void *)heap_ptr;
    heap_ptr += aligned;
    allocated_cnt++;
    return p;
}

void free(void *ptr) {
    (void)ptr;
    if (allocated_cnt > 0u) {
        allocated_cnt--;
    }
    if (allocated_cnt == 0u) {
        /* Bulk-reset bump pointer once every outstanding allocation is gone. */
        heap_ptr = _sheap;
    }
}

__attribute__((noreturn)) void exit(int code) {
    halt(code);
}
