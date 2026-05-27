# Snake SoC Peripheral Devices

> Revised 2026-05-25 (TUI pivot): Framebuffer and Keypad devices removed;
> the SoC now exposes a 5-device set (Boot ROM, UART full-duplex, CLINT,
> IRQ aggregator, DRAM) with the host TTY serving as the single I/O surface.
> SDL2 has been dropped. Snake game pacing migrates to CLINT `mtime`.

This directory contains the C-side implementations of the Snake SoC's
peripheral devices, decoupled from any specific simulator front-end. The
same libraries are used by:

- the **Lab 2 ISS** (links the device libraries directly into the C
  simulator);
- the **Lab 4+ RTL flow** (each device is wrapped by an AXI4-Lite
  SystemVerilog subordinate that calls the same C functions via DPI-C).

See [`/plans/snake-soc.md`](../plans/snake-soc.md) for the locked
device-level contract and [`/CLAUDE.md`](../CLAUDE.md) §3, §7, §8 for the
project-wide conventions this code follows.

## Design pattern

Each device implements one or more **traits** modeled on Rust trait
objects:

- `MmioDevice` (every memory-mapped subordinate)
- `Tickable`   (every module that advances simulator-internal time)

A trait is a struct with a single member — a pointer to a vtable — and
concrete devices implement traits by **embedding** the trait struct as a
named field (`super` for single-trait devices, `mmio_super` /
`tick_super` for multi-trait devices). Trait callbacks recover the
concrete-type pointer via `container_of`. Override methods are named via
the `DECLARE_*` / `SIGNATURE_*` macros so the convention is uniform
across devices.

This is the same pattern as `tmp/ISA-Simulator/`'s `AbstractMem` + `Tick`
idiom, with two refinements:

1. `MmioDevice.read/write` take `(offset, width, value, strb)` — a 1-to-1
   mapping to the AXI4-Lite minimal subset — instead of byte buffers.
2. A new `Ticker` registry parallels `MemoryMap`, removing the legacy
   hard-coded list of tickables inside `ISS_step()`.

Full contract: see [`/plans/snake-soc.md`](../plans/snake-soc.md) §3.

## Directory layout

```
devices/
├── CMakeLists.txt            top-level project (adds_subdirectory for each)
├── README.md                 this file
├── common/                   trait interfaces + registries + terminal backend
│   ├── include/
│   │   ├── common.h          addr_t, container_of, DEV_* macros
│   │   ├── mmio_device.h     MmioDevice trait + DECLARE_* / SIGNATURE_* macros
│   │   ├── tickable.h        Tickable trait + DECLARE_TICKABLE_TICK
│   │   ├── mem_map.h         MemoryMap registry (MmioDevice)
│   │   ├── ticker.h          Ticker registry (Tickable)
│   │   └── terminal_backend.h  Raw-termios stdin/stdout singleton (UART full-duplex)
│   └── src/
│       ├── mmio_device.c     default vtable + dispatchers
│       ├── tickable.c        default vtable + dispatcher
│       ├── mem_map.c         registry with linear-scan dispatch + overlap reject
│       ├── ticker.c          registry with FIFO tick order
│       └── terminal_backend.c  Raw-mode TTY: non-blocking RX, ANSI-aware TX
└── uart/                     TX_DATA / STATUS / RX_DATA (full-duplex over host TTY)
```

## Per-device summary

| Device | Address (base / size) | Traits | External deps |
|---|---|---|---|
| `UART`        | `0x1000_1000` / 12 B   | `MmioDevice` + `Tickable` | host TTY (termios) |

Boot ROM and DRAM are not in this directory — Boot ROM is trivial (8-byte
literal embedded inside the ISS shell) and DRAM is implemented per-Lab
(`MAIN_MEM_IDEAL` for Labs 4–6, abstract model for Lab 7+; the
abstract-model code lives next to the Lab 7 deliverable).

## Build

```bash
mkdir -p build/devices
cmake -S devices -B build/devices
cmake --build build/devices
ctest --test-dir build/devices --output-on-failure
```

No external libraries are required — the terminal backend uses POSIX
`termios` directly. Headless CI redirects stdin/stdout to files or pipes;
the backend detects a non-TTY descriptor and skips the raw-mode setup.

## Integration

Linking a device library into another project:

```cmake
add_subdirectory(${CMAKE_SOURCE_DIR}/devices ${CMAKE_BINARY_DIR}/devices)

add_executable(my_iss src/main.c)
target_link_libraries(my_iss PRIVATE
    devices_common
    devices_uart
)
```

A typical setup wires devices into one `MemoryMap` + one `Ticker`:

```c
#include "uart.h"
#include "mem_map.h"
#include "ticker.h"

UART uart;  UART_ctor(&uart, stdin, stdout);  /* full-duplex */

MemoryMap *map;     MemoryMap_ctor(&map);
Ticker    *ticker;  Ticker_ctor(&ticker);

MemoryMap_add_device(map, (mem_map_unit_t){ 0x10001000u, 12u, UART_as_mmio(&uart) });
Ticker_add(ticker, UART_as_tickable(&uart));   /* polls stdin for RX */

/* ... ISS main loop (terminates on EBREAK) ... */
while (!core.halted) {
    Core_step(core);             /* fetches/loads/stores via map */
    Ticker_tick_all(ticker);     /* drains pending RX bytes, sets RX_VALID */
}
```

## Adding a new device

1. Create `devices/<my_device>/{include,src,tests}` + `CMakeLists.txt`.
2. Define the concrete struct, embedding `MmioDevice super` (and
   `Tickable tick_super` if applicable).
3. Implement override callbacks using the `DECLARE_MMIO_DEVICE_READ(cls)`
   etc. macros from `common/include/mmio_device.h`. Recover the concrete
   pointer via `container_of(self, MyDevice, super)`.
4. In the constructor, call `MmioDevice_ctor(&self->super)` then override
   `self->super.vtbl` with a pointer to a `static const` vtable.
5. If the device implements `Tickable`, repeat (3) and (4) for that
   trait.
6. Add `add_subdirectory(my_device)` to `devices/CMakeLists.txt`.
7. Write at least three tests in `tests/test_<device>.c`:
   read-after-write round-trip, error case, and `MemoryMap` dispatch.

## Conventions

All code in this directory follows [`/CLAUDE.md`](../CLAUDE.md) §10
(C style + defensive programming). Highlights:

- `_Static_assert` for compile-time invariants.
- All ctors validate `self != NULL` via `DEV_ASSERT`.
- Reads of write-only registers and writes to read-only registers both
  return `AXI_RESP_SLVERR` (snake-soc.md §2.5); subordinates never emit
  `AXI_RESP_DECERR` — that is fabric-only.
- `width == 4` with `strb != 0xF` is rejected with `AXI_RESP_SLVERR`
  (matches AXI4-Lite minimal subset).
- No `printf` to stdout — stdout is reserved for UART output.
  All device-side logging goes to stderr via `DEV_LOG` / `DEV_ERR`.
- No threads — the terminal backend is single-threaded; RX polling
  happens on `Tickable.tick` from the simulator main loop
  (see [`/CLAUDE.md`](../CLAUDE.md) §8).
