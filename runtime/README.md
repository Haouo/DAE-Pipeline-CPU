# Snake SoC bare-metal runtime

> Revised 2026-05-25 (TUI pivot): HAL surface narrowed to the 5-device SoC;
> `framebuffer.h` and `keypad.h` are gone, replaced by full-duplex UART (RX/TX)
> as the single I/O path. `halt()` now emits `EBREAK` with the exit code in
> `a0` rather than writing the removed Halt MMIO register.

This directory hosts the bare-metal RV32I runtime that bridges student programs to the Snake SoC hardware. The runtime follows the HAL ⊂ Runtime layering pillar described in `/CLAUDE.md` §5.

## Layering

```
+---------------------------------+
| user main() (Lab 3+ programs)   |
+---------------------------------+
| stdlib/  (string, stdio, malloc)|   <- hardware-INDEPENDENT
+---------------------------------+
| hal/     (uart, clint, irq, halt) |   <- hardware-DEPENDENT (MMIO addresses)
+---------------------------------+
| Snake SoC MMIO                  |
+---------------------------------+
```

Every `stdlib/*.c` translation unit only depends on `hal/*.h` interfaces, never on MMIO addresses or libc. That is what lets the host tests in `host_tests/` exercise printf, malloc, and string verbatim by linking against a mocked HAL.

## Build modes

The CMakeLists.txt auto-detects whether the RV32 cross toolchain is installed:

- If `riscv32-unknown-elf-gcc` is on `PATH`, a static library `libruntime.a` (HAL + stdlib + crt0) is built for the RV target via `toolchain.cmake`.
- If not, only `host_tests/` is built — printf, malloc, and string still pass their unit tests under the host compiler.

```bash
# From the repo root:
cmake -S runtime -B build/runtime
cmake --build build/runtime
ctest --test-dir build/runtime --output-on-failure
```

The Docker image (when published) carries the RV toolchain, so CI exercises both paths.

## File map

| Path | Purpose |
|---|---|
| `link.ld` | Model B linker script — single 64 MB DRAM region at `0x80000000`. |
| `toolchain.cmake` | RV32 cross-compile config. |
| `src/crt/start.S` | Model B crt0 — sets up `gp`/`sp`, zeroes `.bss`, calls `main`. |
| `src/hal/*.c` | MMIO drivers for UART (full-duplex) / CLINT / IRQ aggregator; `halt()` HAL emits `EBREAK` with the exit code in `a0`. |
| `src/stdlib/*.c` | HW-indep string, stdio (printf), bump allocator, assert. |
| `include/hal/*.h` | HAL public surface. |
| `include/stdlib/*.h` | stdlib public surface. |
| `include/runtime.h` | Convenience umbrella. |
| `host_tests/` | Host-only unit tests + HAL mock. |

## Notes

- `printf` supports `%c %d %s %x %u %%`. `%x` is lowercase. `%s` on `NULL` prints `(null)`.
- The bump allocator rounds each request up to a 4-byte boundary using `(size + 3) & ~3u`. It releases the whole heap once every outstanding allocation has been freed (matches the legacy `Base-Runtime` semantics but without the alignment bug).
- `terminate(code)` is the symbol that crt0 tail-calls after `main` returns. It loads `code` into `a0` and executes `EBREAK`; the simulator observes the EBREAK at WB commit and calls `exit(a0 & 0xFF)`.
- Heap and stack are 64 KB each in the link script; revisit when the Snake working set is measured in Lab 8.
