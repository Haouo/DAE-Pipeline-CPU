// Snake-SoC riscv-tests env shim.
//
// Replaces upstream env/p/ which assumes a spike-style host (tohost polling,
// full S-mode + PMP + page-table setup). Pass/fail signals via EBREAK with an
// exit code in a0, matching plans/privileged-arch-plan.md §2.1.
//
// Architecture: RV32I_Zicsr, M-mode only. Single hart. No S-mode, no virtual
// memory, no PMP, no FP, no V extension.

#ifndef _SNAKE_RISCV_TEST_H
#define _SNAKE_RISCV_TEST_H

// CSR field masks and cause codes (MSTATUS_*, MIP_*, CAUSE_*, etc.). Pure
// #define content — safe to pull in even though most symbols here refer to
// features we don't implement.
#include "encoding.h"

//-----------------------------------------------------------------------
// Architecture declaration macros — no-op. The upstream env uses these to
// enable FP / vector / supervisor features; we have none of those.
//-----------------------------------------------------------------------

#define RVTEST_RV32U
#define RVTEST_RV32M
#define RVTEST_RV32S
#define RVTEST_RV32UF
#define RVTEST_RV32UV
#define RVTEST_RV32UVX
#define RVTEST_RV64U
#define RVTEST_RV64M
#define RVTEST_RV64S
#define RVTEST_RV64UF
#define RVTEST_RV64UV
#define RVTEST_RV64UVX

// test_macros.h writes the current test case index into TESTNUM (= gp) before
// each TEST_CASE, then RVTEST_FAIL encodes it into the exit code.
#define TESTNUM gp

//-----------------------------------------------------------------------
// Begin / end markers.
//-----------------------------------------------------------------------
//
// _start sits at the front of .text.init. The reset path installs
// trap_vector into mtvec and falls through into the test body. Tests that
// want a custom trap handler define the weak symbol mtvec_handler;
// trap_vector delegates to it. An otherwise-unhandled trap exits with the
// sentinel code 0xFE.

#define RVTEST_CODE_BEGIN                                       \
        .section .text.init;                                    \
        .align 6;                                               \
        .weak mtvec_handler;                                    \
        .globl _start;                                          \
_start:                                                         \
        la t0, trap_vector;                                     \
        csrw mtvec, t0;                                         \
        li TESTNUM, 0;                                          \
        j _snake_test_body;                                     \
        .align 2;                                               \
trap_vector:                                                    \
        la t5, mtvec_handler;                                   \
        beqz t5, _snake_unhandled_trap;                         \
        jr t5;                                                  \
_snake_unhandled_trap:                                          \
        li a0, 0xFE;                                            \
        ebreak;                                                 \
        .align 2;                                               \
_snake_test_body:

#define RVTEST_CODE_END                                         \
        li a0, 0xFD;                                            \
        ebreak

//-----------------------------------------------------------------------
// Pass / fail signal — EBREAK with exit code in a0[7:0]. snake_soc_main
// observes EBREAK at WB commit and exits the host process with that code.
//-----------------------------------------------------------------------

#define RVTEST_PASS                                             \
        fence;                                                  \
        li   a0, 0;                                             \
        ebreak

#define RVTEST_FAIL                                             \
        fence;                                                  \
1:      beqz TESTNUM, 1b;                                       \
        sll  a0, TESTNUM, 1;                                    \
        ori  a0, a0, 1;                                         \
        ebreak

//-----------------------------------------------------------------------
// Data section markers — no .tohost / .fromhost (no host poll device).
//-----------------------------------------------------------------------

#define EXTRA_DATA

#define RVTEST_DATA_BEGIN                                       \
        EXTRA_DATA                                              \
        .align 4; .global begin_signature; begin_signature:

#define RVTEST_DATA_END                                         \
        .align 4; .global end_signature; end_signature:

#endif // _SNAKE_RISCV_TEST_H
