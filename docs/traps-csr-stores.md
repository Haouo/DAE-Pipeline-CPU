# Traps, CSR, And Stores

This note explains the current side-effect timing.  It is intentionally not a
perfect privileged architecture model.  It is a teaching implementation.

## CSR Timing

CSR read-modify-write instructions are handled in EXE.

EXE receives:

- CSR old value,
- computed CSR new value,
- CSR illegal flag.

The CSR file write is driven from EXE through `csr_commit_o`.  It is suppressed
when MEM is stalling or when a MEM trap is pending.

This timing is simple and matches the current RTL.  A more advanced design could
carry CSR write intent in the token and commit it later, but that is not what
this implementation does today.

## Store Timing

Stores issue their D-memory write request in MEM.  The write enable is gated by:

- token liveness,
- trap/halt checks,
- misalignment checks.

Stores are not delayed to WB, and there is no committed store buffer.

This means the MEM stage is the point where store safety is enforced.

## Trap Timing

MEM detects:

- ECALL,
- enabled machine interrupts,
- selected halt-like conditions used by the testbench model.

When MEM asserts a trap and is not stalled:

- CSR trap-entry state is updated,
- IF redirects to `mtvec_base`,
- frontend epoch increments,
- a kill event is generated from the trapping token,
- younger backend tokens and scoreboard rows are marked killed.

The current implementation reports some events as halt kinds rather than full
architectural traps.  That keeps the teaching core smaller while still allowing
riscv-tests and system-level demos to make progress.

## Why Not Move Everything To WB?

A very clean architecture would make GPR, CSR, and store side effects all commit
from one unified boundary.  That is a good future direction, but it adds new
structures:

- CSR write intent,
- store buffer or committed store path,
- age-based event arbitration,
- stronger token lifetime rules.

The current RTL deliberately stops one step earlier.  It uses token liveness to
make existing EXE/MEM side effects safer and more explicit.  Sequence IDs now
define the kill boundary for younger in-flight backend work, without turning the
core into a ROB-like design.
