/* Tiny printf — extends the legacy %c %d %s formatter with %x (lowercase hex)
 * and %u (unsigned decimal). All output goes through uart_putc(), so a host
 * test that mocks that one symbol fully exercises the formatting code.
 *
 * Hardware-independent — depends only on the UART HAL interface, not on any
 * specific MMIO address.
 */
#include "stdlib/stdio.h"

#include "hal/uart.h"

#include <stdarg.h>
#include <stdint.h>

int putchar(int c) {
    uart_putc((char)c);
    return c;
}

int puts(const char *s) {
    while (*s != '\0') {
        uart_putc(*s++);
    }
    uart_putc('\n');
    return 0;
}

/* Emit a signed decimal int. Handles INT_MIN by working in unsigned. */
static void emit_dec_signed(int value) {
    unsigned int absv;
    if (value < 0) {
        uart_putc('-');
        /* Cast through unsigned to avoid overflow on INT_MIN. */
        absv = (unsigned int)(-(long)value);
    } else {
        absv = (unsigned int)value;
    }
    if (absv >= 10u) {
        emit_dec_signed((int)(absv / 10u));
    }
    uart_putc((char)('0' + (absv % 10u)));
}

static void emit_dec_unsigned(unsigned int value) {
    if (value >= 10u) {
        emit_dec_unsigned(value / 10u);
    }
    uart_putc((char)('0' + (value % 10u)));
}

static void emit_hex_lower(unsigned int value) {
    if (value >= 16u) {
        emit_hex_lower(value >> 4);
    }
    unsigned int nibble = value & 0xFu;
    uart_putc((char)(nibble < 10u ? '0' + nibble : 'a' + (nibble - 10u)));
}

int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt != '\0') {
        if (*fmt != '%') {
            uart_putc(*fmt++);
            continue;
        }
        fmt++; /* skip '%' */
        switch (*fmt) {
            case 'c': {
                /* char promotes to int in va_arg. */
                char c = (char)va_arg(args, int);
                uart_putc(c);
                break;
            }
            case 'd': {
                int v = va_arg(args, int);
                emit_dec_signed(v);
                break;
            }
            case 'u': {
                unsigned int v = va_arg(args, unsigned int);
                emit_dec_unsigned(v);
                break;
            }
            case 'x': {
                unsigned int v = va_arg(args, unsigned int);
                emit_hex_lower(v);
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (s == (const char *)0) {
                    s = "(null)";
                }
                while (*s != '\0') {
                    uart_putc(*s++);
                }
                break;
            }
            case '%': {
                uart_putc('%');
                break;
            }
            case '\0': {
                /* Trailing '%' — bail before fmt++ runs past the terminator. */
                va_end(args);
                return 0;
            }
            default: {
                /* Unknown specifier: emit verbatim for student debuggability. */
                uart_putc('%');
                uart_putc(*fmt);
                break;
            }
        }
        fmt++;
    }
    va_end(args);
    return 0;
}
