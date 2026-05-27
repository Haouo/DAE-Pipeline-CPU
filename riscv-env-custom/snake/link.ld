/* Snake-SoC linker script for riscv-tests.
 *
 * Targets DRAM at 0x80000000 (64 MB). Boot ROM at 0x00000000 contains a
 * hard-coded `lui x1, 0x80000; jalr x0, x1, 0` that jumps to _start here.
 * See plans/snake-soc.md §2 and CLAUDE.md §6 / §7 for the memory map.
 *
 * No .tohost section — pass/fail uses EBREAK, not a polled memory address.
 */

OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY {
    DRAM (rwx) : ORIGIN = 0x80000000, LENGTH = 64M
}

SECTIONS {
    . = 0x80000000;

    .text.init : { *(.text.init) } > DRAM
    .text      : { *(.text*)     } > DRAM

    . = ALIGN(16);
    .rodata    : { *(.rodata*) *(.srodata*) } > DRAM

    . = ALIGN(16);
    .data      : { *(.data*) *(.sdata*) } > DRAM

    . = ALIGN(16);
    .bss       : { *(.bss*) *(.sbss*) *(COMMON) } > DRAM

    _end = .;
}
