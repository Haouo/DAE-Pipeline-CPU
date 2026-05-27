/* Halt HAL — terminates simulation by executing EBREAK with the exit
 * code in a0. The simulator observes EBREAK at the WB commit boundary
 * (plans/privileged-arch-plan.md §2.1) and exits with `a0 & 0xFF`.
 *
 * Replaces the legacy "write 0x10000000 (Halt MMIO)" path, which was
 * removed on 2026-05-24. */

#include "hal/halt.h"

__attribute__((noreturn)) void halt(int code) {
    register int a0 __asm__("a0") = code;
    __asm__ volatile("ebreak" : : "r"(a0));
    /* EBREAK halts the simulator unconditionally. If we ever survive (e.g.
     * running on bare metal without a sim backend), spin so we never fall
     * through into garbage instructions. */
    for (;;) {
        /* nothing */
    }
}

__attribute__((noreturn)) void terminate(int code) {
    halt(code);
}
