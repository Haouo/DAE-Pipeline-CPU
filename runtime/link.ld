/* Snake SoC v0.1 — Model B linker script.
 *
 * Single MEMORY region: all code and data live in DRAM at 0x80000000.
 * No LMA/VMA split — Model B's ELF loader places every PT_LOAD at its p_paddr
 * inside DRAM, then crt0 zeroes .bss. Heap = 64 KB, Stack = 64 KB.
 */

OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    DRAM (rwx) : ORIGIN = 0x80000000, LENGTH = 64M
}

SECTIONS
{
    .text   : ALIGN(4) {
        /* crt0 must be at the entry address (Model B requires e_entry =
         * 0x80000000).  Match .text.start and .text.start.<suffix>, but NOT
         * .text.startup — GCC emits main() into .text.startup as an
         * optimisation, and a broad `.text.start*` glob would pull it in
         * here ahead of crt0. */
        KEEP(*(.text.start));
        KEEP(*(.text.start.*));
        *(.text);
        *(.text.*);
    } > DRAM

    .rodata : ALIGN(4) {
        *(.rodata);
        *(.rodata.*);
    } > DRAM

    .data   : ALIGN(4) {
        PROVIDE(_sdata = .);
        PROVIDE(__global_pointer$ = _sdata + 0x800);
        *(.data);
        *(.data.*);
        PROVIDE(_edata = .);
    } > DRAM

    .bss (NOLOAD) : ALIGN(4) {
        PROVIDE(_sbss = .);
        *(.bss);
        *(.bss.*);
        *(COMMON);
        PROVIDE(_ebss = .);
    } > DRAM

    __heap_size  = 0x10000;  /* 64 KB */
    __stack_size = 0x10000;  /* 64 KB */

    .heap  (NOLOAD) : ALIGN(16) {
        PROVIDE(_sheap = .);
        . += __heap_size;
        PROVIDE(_eheap = .);
    } > DRAM

    .stack (NOLOAD) : ALIGN(16) {
        PROVIDE(_sstack = .);
        . += __stack_size;
        PROVIDE(__stack_top = .);
    } > DRAM

    /DISCARD/ : {
        *(.eh_frame);
        *(.comment);
        *(.debug*);
    }
}
