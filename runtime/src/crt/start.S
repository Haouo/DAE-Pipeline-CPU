/* Model B crt0 — runs at 0x80000000 after the 8-byte Boot ROM stub jumps here.
 *
 * Responsibilities:
 *   1. Set up gp (with .option norelax so the assembler does not rewrite the
 *      load into a gp-relative form before gp is loaded).
 *   2. Set up sp from the linker-provided __stack_top.
 *   3. Zero .bss (the ELF loader does NOT zero p_memsz - p_filesz bytes — that
 *      is the crt0's job per Snake SoC spec §5.3 step 4).
 *   4. Call main(). When it returns, tail-call terminate(a0) so the return
 *      code propagates through the EBREAK voluntary-halt mechanism
 *      (runtime/src/hal/halt.c; plans/privileged-arch-plan.md §2.1).
 */
        .section .text.start, "ax", @progbits
        .global _start
_start:
        .option push
        .option norelax
        la   gp, __global_pointer$
        .option pop
        la   sp, __stack_top

        /* Zero .bss in 4-byte strides. _sbss/_ebss are linker-provided and
         * guaranteed to be 4-byte aligned by the link script. */
        la   t0, _sbss
        la   t1, _ebss
1:
        beq  t0, t1, 2f
        sw   zero, 0(t0)
        addi t0, t0, 4
        j    1b
2:

        /* Hand off to C. main()'s return value sits in a0 already; tail-call
         * terminate so that function gets it as its argument. */
        call main
        tail terminate
