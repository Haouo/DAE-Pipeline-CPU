/* Halt HAL — terminates simulation via EBREAK with exit code in a0.
 *
 * Replaces the legacy "write 0x1000_0000 (Halt MMIO)" path, which was
 * removed on 2026-05-24 (see plans/privileged-arch-plan.md §2.1). */
#ifndef RUNTIME_HAL_HALT_H
#define RUNTIME_HAL_HALT_H

#include <stdint.h>

/* Halt the simulator with the given exit code.
 *   - Mechanism: emits `ebreak` with the low byte of `code` in `a0`. The
 *     simulator observes EBREAK at the WB commit boundary and exits with
 *     `a0 & 0xFF`.
 *   - Fallback: if the sim survives (e.g., running on bare metal without a
 *     sim backend), a spin-loop pins the CPU.
 *   - Threading: call from any context; non-reentrant only in the sense that
 *     control does not return.
 */
__attribute__((noreturn)) void halt(int code);

/* Terminate wrapper used by crt0 after main returns; semantically identical to
 * halt() but kept as a separate symbol so .text.start can `tail terminate`. */
__attribute__((noreturn)) void terminate(int code);

#endif /* RUNTIME_HAL_HALT_H */
