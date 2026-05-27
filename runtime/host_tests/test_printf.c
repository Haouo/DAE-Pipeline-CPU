/* Host smoke tests for stdlib/stdio.c.
 *
 * Every printf() call funnels into the mocked uart_putc(), which appends to a
 * capture buffer. We diff that buffer against the expected string. The mock
 * lives in libmock_hal so all the linker has to do is pull symbols by name.
 */
#include "stdlib/stdio.h"
#include "stdlib/string.h"

#include "mock_hal.h"

#include <assert.h>
#include <stdio.h> /* host libc for sprintf when building the expected string */

static void expect_eq(const char *expected) {
    const char *got = mock_uart_get();
    if (strcmp(got, expected) != 0) {
        fprintf(stderr, "MISMATCH\n  expected: \"%s\"\n  got:      \"%s\"\n",
                expected, got);
        assert(0);
    }
    mock_uart_reset();
}

int main(void) {
    /* %c emits one byte */
    printf("%c%c%c", 'H', 'i', '!');
    expect_eq("Hi!");

    /* %s with literal and embedded format */
    printf("hello %s", "world");
    expect_eq("hello world");

    /* %d covers zero, positive, negative, and INT_MIN-ish boundary */
    printf("%d %d %d", 0, 42, -1);
    expect_eq("0 42 -1");
    printf("%d", -2147483647 - 1); /* INT_MIN */
    expect_eq("-2147483648");

    /* %u — unsigned decimal */
    printf("%u %u", 0u, 4294967295u);
    expect_eq("0 4294967295");

    /* %x — lowercase hex, no leading zeros */
    printf("%x %x %x", 0x0u, 0xdeadbeefu, 0x10u);
    expect_eq("0 deadbeef 10");

    /* Combined "%d %x %s %c %u\n" — the spec's headline example */
    printf("%d %x %s %c %u\n", -7, 0xCAFEu, "snake", 'Z', 100u);
    expect_eq("-7 cafe snake Z 100\n");

    /* %% emits a literal percent */
    printf("100%%");
    expect_eq("100%");

    /* puts adds a newline */
    puts("abc");
    expect_eq("abc\n");

    /* putchar returns its argument */
    int r = putchar('Q');
    assert(r == 'Q');
    expect_eq("Q");

    /* NULL %s prints "(null)" rather than crashing — cast through a volatile
     * pointer so gcc's -Wformat-overflow does not flag it at compile time. */
    {
        const char *volatile p = (const char *)0;
        printf("%s", p);
        expect_eq("(null)");
    }

    return 0;
}
