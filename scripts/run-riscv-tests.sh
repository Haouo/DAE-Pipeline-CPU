#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

VERILATOR_BIN="${VERILATOR_BIN:-/opt/verilator/bin/verilator}"
if [[ ! -x "$VERILATOR_BIN" ]]; then
  VERILATOR_BIN="$(command -v verilator)"
fi

BUILD_DIR="${BUILD_DIR:-$ROOT/obj_dir_dae}"
SOC_BUILD="${SOC_BUILD:-$ROOT/build}"
TEST_DIR="${TEST_DIR:-$SOC_BUILD/riscv-tests}"
MAX_CYCLES="${MAX_CYCLES:-200000}"
SKIP_BUILD="${SKIP_BUILD:-0}"
VERILATOR_PARAMS="${VERILATOR_PARAMS:-}"

if [[ ! -d "$SOC_BUILD" ]]; then
  echo "Missing Snake SoC build directory: $SOC_BUILD" >&2
  echo "Build snake_soc first so libsnake_soc.a and riscv-tests exist." >&2
  exit 1
fi

if [[ ! -d "$TEST_DIR" ]]; then
  echo "Missing riscv-tests directory: $TEST_DIR" >&2
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

mapfile -t TESTS < <(
  find "$TEST_DIR" -maxdepth 1 -type f \
    \( -name 'rv32ui-p-*' -o -name 'rv32mi-p-*' \) | sort
)

if [[ "${#TESTS[@]}" -eq 0 ]]; then
  echo "No rv32ui/rv32mi tests found in $TEST_DIR" >&2
  exit 1
fi

pass=0
for test in "${TESTS[@]}"; do
  name="$(basename "$test")"
  log_file="${TMPDIR:-/tmp}/dae-${name}.log"
  if "$SIM" +ELF="$test" +MAX_CYCLES="$MAX_CYCLES" +QUIET >"$log_file" 2>&1; then
    printf 'PASS %s\n' "$name"
    pass=$((pass + 1))
  else
    printf 'FAIL %s\n' "$name" >&2
    cat "$log_file" >&2
    exit 1
  fi
done

printf 'passed=%d failed=0 total=%d\n' "$pass" "${#TESTS[@]}"
