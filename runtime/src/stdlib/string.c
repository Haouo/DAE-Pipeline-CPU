/* Pure-C implementations of the string.h subset used by the runtime.
 *
 * Hardware-independent — host tests link this object as-is and exercise it
 * with plain C inputs. No HAL dependencies, no libc dependencies.
 */
#include "stdlib/string.h"

#include <stdint.h>

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    unsigned char *d  = (unsigned char *)dst;
    unsigned char  cb = (unsigned char)c;
    for (size_t i = 0; i < n; i++) {
        d[i] = cb;
    }
    return dst;
}

size_t strlen(const char *s) {
    const char *p = s;
    while (*p != '\0') {
        p++;
    }
    return (size_t)(p - s);
}

int strcmp(const char *a, const char *b) {
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    for (; i < n; i++) {
        dst[i] = '\0';
    }
    return dst;
}
