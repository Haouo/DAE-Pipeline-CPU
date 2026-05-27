# tests/riscv-tests — cross-compile upstream riscv-tests for the DPI testbench.
#
# Produces ELFs in ${CMAKE_BINARY_DIR}/riscv-tests/ that scripts/run-riscv-tests.sh
# loads into the Snake SoC DRAM via the DPI bridge. Each ELF is a single
# assembly test from riscv-tests/isa/{rv32ui,rv32mi}/, linked at 0x80000000
# against riscv-env-custom/snake/ (our M-mode-only, EBREAK-exit env shim).
#
# Self-skips if the cross compiler or the riscv-tests submodule is missing.

set(REPO_ROOT       ${CMAKE_SOURCE_DIR})
set(RV_TESTS_SRC    ${REPO_ROOT}/riscv-tests)
set(RV_ENV          ${REPO_ROOT}/riscv-env-custom/snake)
set(RV_OUT_DIR      ${CMAKE_BINARY_DIR}/riscv-tests)

# --- Locate cross compiler ----------------------------------------------
find_program(RISCV_GCC
    NAMES riscv32-unknown-elf-gcc riscv64-unknown-elf-gcc
    HINTS /opt/riscv/bin
)

if(NOT RISCV_GCC)
    message(STATUS "riscv-tests: no riscv*-unknown-elf-gcc found — skipping.")
    return()
endif()

# --- Locate sources -----------------------------------------------------
if(NOT EXISTS ${RV_TESTS_SRC}/isa/rv32ui/Makefrag)
    message(STATUS "riscv-tests: ${RV_TESTS_SRC}/isa not populated "
                   "(run `git submodule update --init --recursive`) — skipping.")
    return()
endif()

if(NOT EXISTS ${RV_TESTS_SRC}/env/encoding.h)
    message(STATUS "riscv-tests: ${RV_TESTS_SRC}/env not populated "
                   "(nested submodule) — skipping.")
    return()
endif()

if(NOT EXISTS ${RV_ENV}/link.ld)
    message(STATUS "riscv-tests: ${RV_ENV}/link.ld missing — skipping.")
    return()
endif()

# --- Test list -----------------------------------------------------------
# Mirrors riscv-tests/isa/rv32ui/Makefrag and rv32mi/Makefrag. Keeping it
# inline (rather than parsing the Makefrags at configure time) keeps the
# CMake side readable; if upstream adds tests, edit here.
set(RV32UI_TESTS
    simple
    add addi
    and andi
    auipc
    beq bge bgeu blt bltu bne
    fence_i
    jal jalr
    lb lbu lh lhu lw ld_st
    lui
    ma_data
    or ori
    sb sh sw st_ld
    sll slli
    slt slti sltiu sltu
    sra srai
    srl srli
    sub
    xor xori
)

set(RV32MI_TESTS
    breakpoint
    csr
    mcsr
    illegal
    ma_fetch
    ma_addr
    scall
    sbreak
    shamt
    lw-misaligned
    lh-misaligned
    sh-misaligned
    sw-misaligned
    zicntr
    instret_overflow
    pmpaddr
)

# --- Compile flags -------------------------------------------------------
set(RV_CFLAGS
    -march=rv32i_zicsr_zifencei
    -mabi=ilp32
    -static
    -mcmodel=medany
    -fvisibility=hidden
    -nostdlib
    -nostartfiles
    -I${RV_ENV}
    -I${RV_TESTS_SRC}/env
    -I${RV_TESTS_SRC}/isa/macros/scalar
)

set(RV_LDFLAGS
    -T${RV_ENV}/link.ld
    -Wl,--no-warn-rwx-segments
)

file(MAKE_DIRECTORY ${RV_OUT_DIR})

set(ALL_RV_ELFS "")

function(_add_rv_test isa_dir prefix name)
    set(elf "${RV_OUT_DIR}/${prefix}-${name}")
    set(src "${RV_TESTS_SRC}/isa/${isa_dir}/${name}.S")
    add_custom_command(
        OUTPUT  ${elf}
        COMMAND ${RISCV_GCC} ${RV_CFLAGS} ${RV_LDFLAGS} ${src} -o ${elf}
        DEPENDS ${src} ${RV_ENV}/link.ld ${RV_ENV}/riscv_test.h
        COMMENT "RV-CC ${prefix}-${name}"
        VERBATIM
    )
    set(ALL_RV_ELFS ${ALL_RV_ELFS} ${elf} PARENT_SCOPE)
endfunction()

foreach(t IN LISTS RV32UI_TESTS)
    _add_rv_test(rv32ui rv32ui-p ${t})
endforeach()

foreach(t IN LISTS RV32MI_TESTS)
    _add_rv_test(rv32mi rv32mi-p ${t})
endforeach()

add_custom_target(riscv_tests ALL DEPENDS ${ALL_RV_ELFS})

list(LENGTH ALL_RV_ELFS _rv_count)
message(STATUS "riscv-tests: building ${_rv_count} tests into ${RV_OUT_DIR} with ${RISCV_GCC}")
