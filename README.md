# DAE Pipeline CPU

DAE Pipeline CPU is a readable RV32I + Zicsr educational core.  It is meant to
show how a simple textbook pipeline can evolve into a more robust design without
jumping all the way to a production out-of-order processor.

The core idea is:

```text
Frontend: IF -> ID -> IR Queue
Backend:  IS/Scoreboard -> EXE -> MEM -> WB
```

The implementation demonstrates:

- frontend/backend decoupling through an IR queue,
- strict in-order issue,
- scoreboard-based hazard detection,
- configurable MEM/WB forwarding,
- branch redirect with an always-not-taken frontend,
- D-cache stall handling,
- CSR and basic trap/interrupt handling,
- epoch-tagged tokens for frontend recovery and stale instruction cleanup.

This is not a production RISC-V core.  It is intentionally small enough to read,
modify, and teach from.

## Repository Layout

```text
rtl/dae_pipeline_cpu/
  include/
    riscv.svh        RISC-V constants
    uarch.svh        microarchitectural types
  src/
    Top.sv           top-level wiring
    frontend.sv      IF/ID/IR queue composition
    if_stage.sv      PC, I-memory request, epoch response filtering
    id_stage.sv      decoder wrapper
    backend.sv       register file, scoreboard, CSR, stage registers
    exe_stage.sv     ALU, branch, CSR RMW computation
    mem_stage.sv     D-memory, trap/halt checks
    wb_stage.sv      GPR writeback and commit packet
    scoreboard.sv    in-order issue hazard logic
    fifo.sv          generic FIFO
  tb/
    tb_top.sv        Verilator testbench
  dpi/
    snake_soc_dpi.*  Snake SoC integration
  docs/
    teaching notes and lab roadmap
  spec.md            implementation spec for the current RTL
```

## Quick Start

Prerequisites used by the current workspace:

- Verilator under `/opt/verilator`
- RISC-V toolchain under `/opt/riscv`
- built Snake SoC libraries under `build/snake_soc`

Run the DAE riscv-tests regression:

```sh
./rtl/dae_pipeline_cpu/scripts/run-riscv-tests.sh
```

By default the script builds a Verilator simulator in `obj_dir_dae` and runs all
available `rv32ui-p-*` and `rv32mi-p-*` tests from
`build/snake_soc/riscv-tests`.

Useful options:

```sh
SKIP_BUILD=1 ./rtl/dae_pipeline_cpu/scripts/run-riscv-tests.sh
MAX_CYCLES=500000 ./rtl/dae_pipeline_cpu/scripts/run-riscv-tests.sh
BUILD_DIR=obj_dir_dae_mf0_wf0 VERILATOR_PARAMS="-GMEM_FORWARDING=0 -GWB_FORWARDING=0" \
  ./rtl/dae_pipeline_cpu/scripts/run-riscv-tests.sh
```

## What To Read First

Start with:

1. [spec.md](spec.md): current implementation contract.
2. [docs/architecture.md](docs/architecture.md): pipeline map and control flow.
3. [docs/classic-5-stage-to-dae.md](docs/classic-5-stage-to-dae.md): why this
   design is more structured than a classic 5-stage pipeline.
4. [docs/epoch-token-recovery.md](docs/epoch-token-recovery.md): the epoch/token
   idea used by the frontend.
5. [docs/lab-roadmap.md](docs/lab-roadmap.md): suggested teaching sequence.

## Educational Positioning

This core is useful for teaching the gap between:

- the clean five-stage pipeline from textbooks, and
- the control discipline required once cache stalls, traps, CSRs, and redirects
  all exist at the same time.

The design keeps the implementation in-order and readable, but introduces a few
ideas that scale better than ad hoc stage control:

- explicit decoded IR packets,
- stage-local tokens,
- frontend epochs,
- scoreboard rows,
- centralized redirect/trap priority.

## Current Limitations

- No ROB.
- No branch predictor beyond always-not-taken.
- No real cache hierarchy.
- Scoreboard is still tied to EXE/MEM/WB rows.
- CSR writes happen from EXE.
- Stores issue from MEM.
- Privilege behavior is M-mode oriented and intentionally smaller than a full
  privileged architecture implementation.

These limitations are deliberate teaching opportunities, not accidental
omissions.

