/* Host smoke tests for stdlib/stdlib.c bump allocator.
 *
 * Verifies:
 *   1. malloc returns a non-NULL pointer inside [_sheap, _eheap).
 *   2. The (size + 3) & ~3u alignment rule is honored (gap between two
 *      consecutive allocations of an odd-byte size is rounded up to 4).
 *      This is the explicit regression for the legacy `(size + 3) % 4` bug.
 *   3. malloc beyond _eheap returns NULL.
 *   4. free() once-per-malloc resets the bump pointer after the last one.
 */
#include "stdlib/stdlib.h"
#include "stdlib/string.h"

#include "mock_hal.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

extern char _sheap[];
extern char _eheap[];

int main(void) {
    mock_heap_reset();

    /* (1) basic allocation in-range */
    void *p = malloc(8);
    assert(p != (void *)0);
    assert((char *)p >= _sheap && (char *)p < _eheap);

    /* (2) alignment regression — request 1 byte, then 1 byte, then check that
     * the second allocation is 4 bytes after the first. With the legacy bug
     * `(1 + 3) % 4 == 0`, malloc would bump by 0 bytes and return the same
     * address. The fix `(1 + 3) & ~3u == 4` produces the correct stride. */
    mock_heap_reset();
    char *a = (char *)malloc(1);
    char *b = (char *)malloc(1);
    assert(a != (void *)0 && b != (void *)0);
    if (b - a != 4) {
        fprintf(stderr, "alignment regression: stride was %ld, expected 4\n",
                (long)(b - a));
        assert(0);
    }

    /* Same check with size=5, expect stride of 8. */
    mock_heap_reset();
    char *c = (char *)malloc(5);
    char *d = (char *)malloc(5);
    assert(c != (void *)0 && d != (void *)0);
    assert(d - c == 8);

    /* (3) oversize allocation returns NULL */
    mock_heap_reset();
    size_t heap_bytes = mock_heap_size();
    void *huge = malloc(heap_bytes + 16);
    assert(huge == (void *)0);

    /* Many small allocations until we run out — last one is NULL. */
    mock_heap_reset();
    size_t alloc_count = 0;
    while (malloc(64) != (void *)0) {
        alloc_count++;
        if (alloc_count > heap_bytes) {
            assert(0); /* safety: did not terminate */
        }
    }
    assert(alloc_count == heap_bytes / 64u);

    /* (4) free-then-malloc round trip */
    mock_heap_reset();
    void *p1 = malloc(32);
    void *p2 = malloc(32);
    assert(p1 != (void *)0 && p2 != (void *)0);
    free(p1);
    free(p2);
    /* After all frees, the next malloc should land at _sheap again. */
    void *p3 = malloc(16);
    assert((char *)p3 == _sheap);

    /* Writing through a returned pointer must be safe — exercise the storage. */
    memset(p3, 0x5A, 16);
    for (int i = 0; i < 16; i++) {
        assert(((unsigned char *)p3)[i] == 0x5A);
    }

    return 0;
}
