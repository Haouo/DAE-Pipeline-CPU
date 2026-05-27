# DAE Pipeline CPU Implementation Spec

This document describes the current RTL implementation in this repository.
It is an implementation spec, not a full ISA spec.  Instruction semantics are
defined by the reference ISS and by the decoder/CSR behavior in the RTL.

## Architecture Summary

The CPU is an in-order, single-issue RV32I + Zicsr pipeline with a decoupled
frontend and backend.

```text
Frontend: IF -> ID -> IR Queue
Backend:  IS/Scoreboard -> EXE -> MEM -> WB
```

The frontend and backend communicate through an IR queue containing
`decoded_ir_t`.  The backend issues at most one IR per cycle, strictly from the
queue head.

The current design uses epoch-tagged instruction tokens for frontend recovery
and wrong-path validity.  It does not implement a ROB.  CSR writes still happen
from EXE through `CSRFile`, and store writes still happen from MEM through the
D-cache request path.

## Top-Level Parameters

`Top` exposes these implementation parameters:

| Parameter | Default | Meaning |
|---|---:|---|
| `MEM_FORWARDING` | `1` | Enables MEM-row result forwarding to IS operands. |
| `WB_FORWARDING` | `1` | Enables WB-row result forwarding to IS operands. |
| `IR_QUEUE_DEPTH` | `4` | Number of decoded IR entries buffered between frontend and backend. |
| `RESET_PC` | `32'h0` | Reset PC. |

## Main Data Types

Defined in `include/uarch.svh`.

### Epoch and Sequence IDs

```systemverilog
parameter int EPOCH_W = 2;
parameter int SEQ_ID_W = 8;

typedef logic [EPOCH_W-1:0] epoch_t;
typedef logic [SEQ_ID_W-1:0] seq_id_t;
```

`epoch_t` identifies the currently valid speculative frontend generation.
Redirects increment the current epoch.  Old epoch packets may remain in local
storage, but they must not issue or commit.

`seq_id_t` is allocated by the frontend when an I-cache request is issued.  It
is carried through fetch/decode/token packets for identity and future control
extensions.  The current scoreboard is still stage-row based and does not use
`seq_id_t` for hazard detection.

### `fetch_pkt_t`

Carries a raw instruction from IF to ID:

- `valid`
- `epoch`
- `seq_id`
- `pc`
- `inst`
- `error`

### `decoded_ir_t`

Carries decoded instruction information from ID to the IR queue and then to IS.
It contains the epoch/sequence identity, PC, raw instruction, FU kind, operands,
immediates, CSR fields, memory fields, and decoder illegal flag.

No downstream stage re-decodes the raw instruction except for small opcode
checks that distinguish JALR target behavior.

### `inst_token_t`

The backend pipeline register payload:

- `valid`, `epoch`, `seq_id`
- decoded IR
- source operand values
- ALU/memory/CSR result fields
- store address/data/strobe metadata
- `killed`
- `trap_pending`, `trap_cause`, `trap_tval`
- halt metadata

Side-effecting stages use `token_alive = valid && !killed` before producing
redirects, traps, stalls, CSR writes, stores, GPR writes, or commit packets.

## Frontend

The frontend consists of `dae_if_stage`, `dae_id_stage`, and a generic FIFO IR
queue.

### IF Stage

`dae_if_stage` maintains the PC and supports one outstanding I-cache request.
The fetch policy is always not-taken:

- Normal request address is `pc_q`.
- On accepted request, `pc_q` increments by 4.
- On redirect, `pc_q` becomes `redirect_pc_i`.

Each I-cache request records:

- request PC
- current epoch
- next sequence ID

When an I-cache response arrives, IF emits a `fetch_pkt_t` only if:

- the response was not marked for redirect drop,
- no current flush/redirect is active, and
- the request epoch still equals `current_epoch_i`.

Otherwise the response is discarded.

### ID Stage

`dae_id_stage` wraps `decoder` and copies `epoch` and `seq_id` from the fetch
packet into the decoded IR.

### IR Queue and Epoch Recovery

`dae_frontend` owns:

- `current_epoch_q`
- `next_seq_id_q`

On redirect, `current_epoch_q` increments.  On accepted I-cache request,
`next_seq_id_q` increments.

The IR queue is not physically flushed on redirect in the current design:

```systemverilog
fifo.flush = 1'b0;
```

Instead, the queue head is considered stale when:

```systemverilog
queue_valid && queue_ir.epoch != current_epoch_q
```

Stale queue entries are dequeued automatically and are not presented as valid
backend candidates.

## Backend

The backend contains:

- register file
- scoreboard
- CSR file
- EXE, MEM, and WB stages
- pipeline registers `exe_q`, `mem_q`, `wb_q`

The backend receives only the current IR queue head.  Issue remains strict
in-order.

### Scoreboard and IS Behavior

The scoreboard has three stage rows:

- EXE row
- MEM row
- WB row

Each row tracks RS debug fields, RD destination, RD readiness, and written-back
state.  Hazard detection compares candidate RS1/RS2 against in-flight RD fields.

Forwarding behavior:

- EXE-to-IS forwarding is not implemented.
- MEM-to-IS forwarding is controlled by `MEM_FORWARDING`.
- WB-to-IS forwarding is controlled by `WB_FORWARDING`.

If a dependency cannot be forwarded, `issue_valid` deasserts and the IR queue is
not popped.  On successful issue, IS creates an `inst_token_t` containing the
decoded IR identity and forwarded operand values.

### EXE Stage

`dae_exe_stage` performs:

- integer ALU operations,
- branch/jump resolution,
- JAL/JALR link value production,
- CSR read-modify-write value computation,
- MRET redirect generation,
- branch target misalignment halt detection.

EXE emits redirects for taken branches, JAL/JALR, and MRET when the token is
alive and MEM is not stalling/trapping.

CSR timing in the current RTL:

- CSR old/new values are read/computed in EXE.
- CSR writes are driven into `CSRFile` from EXE via `csr_commit_o`.
- CSR writes are suppressed when MEM is stalling or a MEM trap is pending.

### MEM Stage

`dae_mem_stage` performs:

- load/store address checking,
- D-cache request generation,
- load data filtering,
- synchronous trap/halt detection,
- interrupt sampling,
- trap-pending annotation on tokens.

Memory timing in the current RTL:

- Load and store D-cache requests are generated in MEM.
- Store `we` is gated by trap/halt/misalignment checks.
- Store requests are not delayed to WB.
- A memory token stalls MEM until its response arrives.

Trap behavior:

- ECALL and enabled external/timer/software interrupts set `trap_pending`.
- Illegal instructions, bus errors, EBREAK, misalignment, and double-trap-like
  cases are represented as halt conditions in this implementation.
- When `take_trap_o` is asserted and MEM is not stalled, backend flush/redirect
  is generated.

### WB Stage

`dae_wb_stage` commits GPR writeback and emits `commit_packet_t`.

WB commit is valid only when:

```systemverilog
wb_i.valid && !wb_i.killed
```

GPR writes are suppressed for:

- trap-pending tokens,
- halted tokens,
- `rd == x0`,
- instructions with no RD write.

WB also reports store metadata and CSR snapshots through `commit_packet_t` for
testbench/ISS comparison.

## Control Events and Priority

There are three major backend events.

### MEM Stall

Source: MEM memory access waiting for D-cache response.

Effects:

- scoreboard holds,
- EXE/MEM pipeline registers are held,
- WB drains with a bubble,
- frontend continues fetching/decoding until the IR queue fills.

### EXE Redirect

Source: branch/JAL/JALR/MRET in EXE.

Effects:

- frontend redirect is asserted,
- frontend epoch increments,
- IF PC changes to redirect target,
- stale frontend/IR-queue entries naturally drain by epoch mismatch,
- backend scoreboard is not flushed.

### MEM Trap Redirect

Source: trap detected in MEM, when MEM is not stalled.

Effects:

- `backend_flush` asserts,
- scoreboard flushes all rows,
- EXE and MEM tokens are cleared,
- WB receives the computed MEM token for current-cycle commit reporting,
- frontend redirect is asserted,
- frontend epoch increments,
- IF PC redirects to `mtvec_base`.

Trap redirect has priority over EXE redirect:

```systemverilog
redirect_valid = backend_flush || exe_redirect_valid;
redirect_pc    = backend_flush ? csr_mtvec_base : exe_redirect_pc;
```

## Epoch/Token Validity Rules

The implemented epoch/token discipline is intentionally small:

1. Fetch requests are tagged with the current epoch.
2. Fetch responses whose request epoch is stale are discarded.
3. Decoded IR carries epoch and sequence ID.
4. IR queue entries with stale epoch are silently popped.
5. Backend tokens carry epoch/sequence ID and a `killed` bit.
6. Current backend side effects are gated by token liveness, not by ad hoc raw
   `valid` checks alone.

The current design does not yet perform age-based event arbitration with
`seq_id_t`, and it does not maintain a ROB or committed store buffer.

## Interfaces

### Memory Request/Response

Both I-memory and D-memory use `mem_req_t` / `mem_resp_t`.

`mem_req_t`:

- `req_valid`
- `addr`
- `we`
- `be`
- `wdata`

`mem_resp_t`:

- `resp_valid`
- `rdata`
- `error`

The I-side supports one outstanding request in IF.  The D-side is blocking and
tracks one outstanding request in MEM.

### Interrupt Inputs

The backend accepts:

- `msip_i`
- `mtip_i`
- `meip_i`

`CSRFile` reflects them in `mip`.  MEM samples interrupts when `mstatus.MIE` and
the corresponding `mie/mip` bits are enabled.

## Halt Model

This RTL includes a simple halt model for testbench integration.  Some
conditions that a full privileged implementation would model as traps are
reported as halt kinds:

- illegal instruction,
- IF/load/store bus error,
- EBREAK,
- misaligned PC/load/store,
- double-trap-like cases.

Voluntary riscv-test completion uses EBREAK and the value in x10 as the exit
code.

## Current Limitations

- No ROB.
- No age-based event arbitration using `seq_id_t`.
- Scoreboard is still tied to EXE/MEM/WB rows.
- CSR writes still occur from EXE, not from a unified commit stage.
- Stores still issue from MEM, not from a committed store buffer.
- Epoch recovery is currently frontend-oriented; backend wrong-path cleanup is
  still primarily handled by existing trap flush and branch non-flush policy.
- Privilege behavior is M-mode oriented and intentionally smaller than a full
  privileged architecture implementation.

## Verification Status

At the time this spec was written, the current implementation passed:

```text
Verilator lint
Verilator testbench build
43 rv32ui/rv32mi riscv-tests
```

The testbench entry point is `tb/tb_top.sv`, with Snake SoC DPI integration in
`dpi/snake_soc_dpi.c`.
