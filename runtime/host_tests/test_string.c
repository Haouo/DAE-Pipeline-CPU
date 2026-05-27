/* Host smoke tests for stdlib/string.c. */
#include "stdlib/string.h"

#include <assert.h>
#include <string.h> /* host libc strcmp/strlen for cross-check; allowed in test code */

int main(void) {
    /* memcpy round-trip */
    char src[16] = "hello world!!!!";
    char dst[16] = {0};
    memcpy(dst, src, sizeof(src));
    for (size_t i = 0; i < sizeof(src); i++) {
        assert(dst[i] == src[i]);
    }

    /* memset fills every byte */
    char buf[8];
    memset(buf, 0xA5, sizeof(buf));
    for (size_t i = 0; i < sizeof(buf); i++) {
        assert((unsigned char)buf[i] == 0xA5);
    }

    /* strlen against libc reference */
    assert(strlen("") == 0);
    assert(strlen("a") == 1);
    assert(strlen("hello") == 5);

    /* strcmp ordering and equality */
    assert(strcmp("abc", "abc") == 0);
    assert(strcmp("abc", "abd") < 0);
    assert(strcmp("abd", "abc") > 0);
    assert(strcmp("", "") == 0);
    assert(strcmp("a", "") > 0);

    /* strncpy truncates and zero-pads */
    char d[6] = {'X', 'X', 'X', 'X', 'X', 'X'};
    strncpy(d, "hi", sizeof(d));
    assert(d[0] == 'h' && d[1] == 'i');
    for (size_t i = 2; i < sizeof(d); i++) {
        assert(d[i] == '\0');
    }
    char d2[3] = {0};
    strncpy(d2, "hello", sizeof(d2));   /* no terminator written */
    assert(d2[0] == 'h' && d2[1] == 'e' && d2[2] == 'l');

    return 0;
}
