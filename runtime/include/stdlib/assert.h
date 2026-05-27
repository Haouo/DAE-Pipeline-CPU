/* Runtime assertion macro — prints location + condition then halts.
 *
 * Named RUNTIME_ASSERT (not assert) so it does not collide with newlib's
 * <assert.h> if a host-build test happens to pull both in.
 */
#ifndef RUNTIME_STDLIB_ASSERT_H
#define RUNTIME_STDLIB_ASSERT_H

#include "stdio.h"
#include "../hal/halt.h"

#define RUNTIME_ASSERT(cond)                                                   \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("ASSERT FAIL: %s:%d %s\n", __FILE__, __LINE__, #cond);      \
            halt(1);                                                           \
        }                                                                      \
    } while (0)

#endif /* RUNTIME_STDLIB_ASSERT_H */
