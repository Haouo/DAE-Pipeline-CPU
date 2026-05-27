#ifndef DEVICES_COMMON_H
#define DEVICES_COMMON_H

/* Shared utilities for every device library and the registries.
 *
 * Includes:
 *   - addr_t typedef (32-bit physical address; matches Snake SoC address space)
 *   - container_of macro (copied verbatim from tmp/ISA-Simulator/src/common.h
 *     to preserve OOP-in-C trait composition across the project)
 *   - DEV_LOG / DEV_ERR / DEV_PANIC / DEV_ASSERT logging macros (all to stderr;
 *     stdout is reserved for UART output per /CLAUDE.md §8)
 *   - errno-style return-code values via <errno.h>
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* 32-bit physical address space used by the Snake SoC; see
 * plans/snake-soc.md §2.1. */
typedef uint32_t addr_t;

#ifndef offsetof
#    define offsetof(type, member) __builtin_offsetof(type, member)
#endif

/* container_of(ptr, type, member)
 *   ptr    — pointer to the `member` field
 *   type   — outer struct type
 *   member — name of the field inside `type`
 *
 * Returns a `type *` to the outer struct that contains `ptr`. The
 * `__typeof__` dance produces a temporary copy with the right pointer
 * type so the compiler can validate the cast at compile time.
 */
#define container_of(ptr, type, member)                             \
    __extension__({                                                 \
        const __typeof__(((type *)0)->member) *(__pmember) = (ptr); \
        (type *)((char *)__pmember - offsetof(type, member));       \
    })

#define DEV_LOG(fmt, ...) fprintf(stderr, "[dev] " fmt "\n", ##__VA_ARGS__)

#define DEV_ERR(fmt, ...) \
    fprintf(stderr, "[dev:err] " fmt " (%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__)

#define DEV_PANIC(fmt, ...)          \
    do {                             \
        DEV_ERR(fmt, ##__VA_ARGS__); \
        abort();                     \
    } while (0)

#define DEV_ASSERT(cond, fmt, ...)                                          \
    do {                                                                    \
        if (!(cond)) {                                                      \
            DEV_PANIC("assertion failed: " #cond " — " fmt, ##__VA_ARGS__); \
        }                                                                   \
    } while (0)

#endif /* DEVICES_COMMON_H */
