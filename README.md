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
include/
  riscv.svh             RISC-V constants
  uarch.svh             microarchitectural types
src/
  Top.sv                top-level wiring
  frontend.sv           IF/ID/IR queue composition
  if_stage.sv           PC, I-memory request, epoch response filtering
  id_stage.sv           decoder wrapper
  backend.sv            register file, scoreboard, CSR, stage registers
  exe_stage.sv          ALU, branch, CSR RMW computation
  mem_stage.sv          D-memory, trap/halt checks
  wb_stage.sv           GPR writeback and commit packet
  scoreboard.sv         in-order issue hazard logic
  fifo.sv               generic FIFO
tb/
  tb_top.sv             Verilator testbench
dpi/
  snake_soc_dpi.*       Snake SoC DPI bridge (memory map, ticker, mip wires)
docs/
  teaching notes and lab roadmap
spec.md                 implementation spec for the current RTL

snake_soc/              C-side SoC composition (peripherals + memory map)
devices/                BootROM / CLINT / UART / IRQ-AGG / DRAM
elf_loader/             ELF -> MemoryMap loader
riscv-tests/            upstream submodule
riscv-env-custom/       custom riscv-tests env (snake)
tests/riscv-tests/      CMake module that cross-compiles riscv-tests
FreeRTOS-Kernel/        FreeRTOS source for the demo
freertos_demo/          FreeRTOS demo app
CMakeLists.txt          top-level build (snake_soc + riscv-tests)
```

## Quick Start

Prerequisites:

- Verilator under `/opt/verilator`
- RISC-V toolchain under `/opt/riscv` (`riscv64-unknown-elf-gcc` is fine for
  rv32 with `-march=rv32i_zicsr_zifencei -mabi=ilp32`)
- riscv-tests submodule initialized:

  ```sh
  git submodule update --init --recursive
  ```

Build the Snake SoC C backend (libsnake_soc.a + device libs) and cross-compile
the riscv-tests in one shot:

```sh
cmake -S . -B build
cmake --build build -j
```

This produces:

- `build/snake_soc/libsnake_soc.a`, `build/_devices/*/libdevices_*.a`,
  `build/_elf_loader/libelf_loader.a` — linked into the DPI testbench.
- `build/riscv-tests/rv32{ui,mi}-p-*` — 58 ELFs loaded by the testbench.

Cross-compilation self-skips with a status message if `riscv*-unknown-elf-gcc`
or the riscv-tests submodule is missing; set `-DBUILD_RISCV_TESTS=OFF` to skip
explicitly.

Run the DAE riscv-tests regression:

```sh
./scripts/run-riscv-tests.sh
```

By default the script builds a Verilator simulator in `obj_dir_dae` and runs all
available `rv32ui-p-*` and `rv32mi-p-*` tests from `build/riscv-tests`. Override
`SOC_BUILD` if you build into a different directory, or `TEST_DIR` if your
riscv-tests ELFs live elsewhere.

Useful options:

```sh
SKIP_BUILD=1 ./scripts/run-riscv-tests.sh
MAX_CYCLES=500000 ./scripts/run-riscv-tests.sh
BUILD_DIR=obj_dir_dae_mf0_wf0 VERILATOR_PARAMS="-GMEM_FORWARDING=0 -GWB_FORWARDING=0" \
  ./scripts/run-riscv-tests.sh
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

