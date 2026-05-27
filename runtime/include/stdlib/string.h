/* Minimal freestanding <string.h> for the Snake SoC runtime. */
#ifndef RUNTIME_STDLIB_STRING_H
#define RUNTIME_STDLIB_STRING_H

#include <stddef.h>

void  *memcpy (void *dst, const void *src, size_t n);
void  *memset (void *dst, int c, size_t n);
size_t strlen (const char *s);
int    strcmp (const char *a, const char *b);
char  *strncpy(char *dst, const char *src, size_t n);

#endif /* RUNTIME_STDLIB_STRING_H */
