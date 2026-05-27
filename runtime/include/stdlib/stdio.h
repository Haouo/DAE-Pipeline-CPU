/* Minimal freestanding <stdio.h> built on top of the UART HAL.
 *
 * All output ultimately funnels through uart_putc() so host tests can capture
 * the stream by mocking that single symbol. printf supports %c %d %s %x %u.
 */
#ifndef RUNTIME_STDLIB_STDIO_H
#define RUNTIME_STDLIB_STDIO_H

int putchar(int c);
int puts   (const char *s);
int printf (const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* RUNTIME_STDLIB_STDIO_H */
