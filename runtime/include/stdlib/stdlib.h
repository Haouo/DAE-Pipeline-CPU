/* Minimal freestanding <stdlib.h> for the Snake SoC runtime.
 *
 * Implements a bump allocator backed by the linker-provided heap region
 * [_sheap, _eheap). free() decrements a refcount and resets the bump pointer
 * once every outstanding allocation has been released — matching the legacy
 * Base-Runtime semantics but with the (size + 3) & ~3u alignment fix.
 *
 * Host-build note: when compiled with -DRUNTIME_HOST_BUILD (set by
 * host_tests/CMakeLists.txt), the public symbols are renamed to rt_malloc /
 * rt_free / rt_exit so they do not shadow the host libc's allocator. Tests
 * still call them as malloc/free/exit thanks to the macros below. On the RV
 * target the macros are absent and the symbols ARE the public names.
 */
#ifndef RUNTIME_STDLIB_STDLIB_H
#define RUNTIME_STDLIB_STDLIB_H

#include <stddef.h>

#ifdef RUNTIME_HOST_BUILD
#define malloc rt_malloc
#define free   rt_free
#define exit   rt_exit
#endif

void *malloc(size_t size);
void  free  (void *ptr);
__attribute__((noreturn)) void exit(int code);

#endif /* RUNTIME_STDLIB_STDLIB_H */
