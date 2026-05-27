#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

VERILATOR_BIN="${VERILATOR_BIN:-/opt/verilator/bin/verilator}"
if [[ ! -x "$VERILATOR_BIN" ]]; then
  VERILATOR_BIN="$(command -v verilator)"
fi

BUILD_DIR="${BUILD_DIR:-$ROOT/obj_dir_dae}"
SOC_BUILD="${SOC_BUILD:-$ROOT/build}"
FREERTOS_BUILD="${FREERTOS_BUILD:-$ROOT/build/freertos_demo}"
FREERTOS_ELF="${FREERTOS_ELF:-$FREERTOS_BUILD/freertos_demo}"
MAX_CYCLES="${MAX_CYCLES:-200000}"
SKIP_BUILD="${SKIP_BUILD:-0}"
SKIP_FREERTOS_BUILD="${SKIP_FREERTOS_BUILD:-0}"
VERILATOR_PARAMS="${VERILATOR_PARAMS:-}"
LOG_FILE="${LOG_FILE:-${TMPDIR:-/tmp}/dae-freertos-demo.log}"

if [[ ! -d "$SOC_BUILD" ]]; then
  echo "Missing Snake SoC build directory: $SOC_BUILD" >&2
  echo "Build snake_soc first so libsnake_soc.a and device libs exist." >&2
  exit 1
fi

SV_FILES=(
  "$ROOT/src/fifo.sv"
  "$ROOT/src/regfile.sv"
  "$ROOT/src/decoder.sv"
  "$ROOT/src/scoreboard.sv"
  "$ROOT/src/alu.sv"
  "$ROOT/src/branch_unit.sv"
  "$ROOT/src/CSRFile.sv"
  "$ROOT/src/lsu.sv"
  "$ROOT/src/load_data_filter.sv"
  "$ROOT/src/if_stage.sv"
  "$ROOT/src/id_stage.sv"
  "$ROOT/src/frontend.sv"
  "$ROOT/src/exe_stage.sv"
  "$ROOT/src/mem_stage.sv"
  "$ROOT/src/wb_stage.sv"
  "$ROOT/src/backend.sv"
  "$ROOT/src/Top.sv"
  "$ROOT/tb/tb_top.sv"
  "$ROOT/dpi/snake_soc_dpi.c"
)

CFLAGS="-I$ROOT/dpi"
CFLAGS+=" -I$ROOT/snake_soc/include"
CFLAGS+=" -I$ROOT/devices/common/include"
CFLAGS+=" -I$ROOT/devices/boot_rom/include"
CFLAGS+=" -I$ROOT/devices/uart/include"
CFLAGS+=" -I$ROOT/devices/dram/include"
CFLAGS+=" -I$ROOT/devices/clint/include"
CFLAGS+=" -I$ROOT/devices/irq_agg/include"
CFLAGS+=" -I$ROOT/elf_loader/include"

LDFLAGS="$SOC_BUILD/snake_soc/libsnake_soc.a"
LDFLAGS+=" $SOC_BUILD/_devices/boot_rom/libdevices_boot_rom.a"
LDFLAGS+=" $SOC_BUILD/_devices/clint/libdevices_clint.a"
LDFLAGS+=" $SOC_BUILD/_devices/uart/libdevices_uart.a"
LDFLAGS+=" $SOC_BUILD/_devices/dram/libdevices_dram.a"
LDFLAGS+=" $SOC_BUILD/_devices/irq_agg/libdevices_irq_agg.a"
LDFLAGS+=" $SOC_BUILD/_elf_loader/libelf_loader.a"
LDFLAGS+=" $SOC_BUILD/_devices/common/libdevices_common.a"

if [[ "$SKIP_FREERTOS_BUILD" != "1" ]]; then
  cmake -S "$ROOT/freertos_demo" -B "$FREERTOS_BUILD" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/runtime/toolchain.cmake"
  cmake --build "$FREERTOS_BUILD" -j
fi

if [[ ! -f "$FREERTOS_ELF" ]]; then
  echo "Missing FreeRTOS ELF: $FREERTOS_ELF" >&2
  exit 1
fi

if [[ "$SKIP_BUILD" != "1" ]]; then
  # shellcheck disable=SC2086
  "$VERILATOR_BIN" --binary -sv \
    --Mdir "$BUILD_DIR" \
    -I"$ROOT/include" \
    ${VERILATOR_PARAMS} \
    "${SV_FILES[@]}" \
    --top-module tb_top \
    -CFLAGS "$CFLAGS" \
    -LDFLAGS "$LDFLAGS"
fi

SIM="$BUILD_DIR/Vtb_top"
if [[ ! -x "$SIM" ]]; then
  echo "Missing simulator binary: $SIM" >&2
  exit 1
fi

if "$SIM" +ELF="$FREERTOS_ELF" +MAX_CYCLES="$MAX_CYCLES" +QUIET +ALLOW_TIMEOUT >"$LOG_FILE" 2>&1; then
  if ! grep -q "\[boot\] main entered" "$LOG_FILE"; then
    echo "FAIL freertos_demo: main did not boot" >&2
    cat "$LOG_FILE" >&2
    exit 1
  fi
  if ! grep -q "\[boot\] scheduler starting" "$LOG_FILE"; then
    echo "FAIL freertos_demo: scheduler did not start" >&2
    cat "$LOG_FILE" >&2
    exit 1
  fi
  if ! grep -q "tick 0" "$LOG_FILE"; then
    echo "FAIL freertos_demo: tasks did not tick" >&2
    cat "$LOG_FILE" >&2
    exit 1
  fi

  cat "$LOG_FILE"
  printf 'PASS freertos_demo boot max_cycles=%d\n' "$MAX_CYCLES"
else
  echo "FAIL freertos_demo" >&2
  cat "$LOG_FILE" >&2
  exit 1
fi
