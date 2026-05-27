/* Host-side _sheap / _eheap symbols for the bump allocator.
 *
 * The runtime allocator declares these as `extern char _sheap[]` and
 * `extern char _eheap[]` and treats them as ADDRESS-valued symbols (the
 * boundary of the heap region). On the RV target the linker script places
 * them. On the host we want byte-exact control over both addresses, which
 * is what a tiny assembly file gives us — the C compiler is free to reorder
 * data definitions, but the assembler emits labels exactly where written.
 *
 * Layout: 64 KiB reserve, _sheap at base, _eheap immediately past the end.
 * The _eheap address must not be dereferenced — the allocator only compares
 * against it.
 */
        .data
        .balign 16
        .globl  _sheap
_sheap:
        .zero   65536
        .globl  _eheap
_eheap:
        .byte   0       /* sentinel — never read or written */
