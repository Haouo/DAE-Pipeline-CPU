cmake_minimum_required(VERSION 3.21...3.28)

# FreeRTOS-on-ISS demo.
#
# Builds a freestanding RV32I_Zicsr ELF that boots the FreeRTOS-Kernel
# RISC-V port (V11.3.0) on the Snake SoC running inside the Lab 2 ISS.
#
# Composition:
#   - kernel:   FreeRTOS list/queue/tasks/timers/heap_4 + GCC/RISC-V port
#   - chip ext: RISCV_MTIME_CLINT_no_extensions  (matches our SiFive-style
#               CLINT at 0x0200_0000 with mtime / mtimecmp / writable msip)
#   - HAL:      runtime/ (uart full-duplex, halt) + crt0 + linker
#   - glue:     port_glue/  (app interrupt handler that drains IRQ-Agg,
#               FreeRTOS hooks, mtvec install helper)
#   - app:      app/main.c   (3 tasks demonstrating MTI / ECALL yield /
#               MEI from UART RX-valid edge — post-TUI-pivot 2026-05-25)
#
# Invocation:
#   cmake -S freertos_demo -B build/freertos_demo \
#         -DCMAKE_TOOLCHAIN_FILE=../runtime/toolchain.cmake
#   cmake --build build/freertos_demo
#
# Run on the ISS:
#   ./build/snake_soc_main build/freertos_demo/freertos_demo.elf
#
# This project is intentionally standalone — it is NOT part of test-progs/
# nor of any lab-N/ since it sits in Bonus B territory (CLAUDE.md §6).

project(freertos_demo LANGUAGES NONE)

# Toolchain detection — same soft-fail as test-progs/.
if(NOT DEFINED CMAKE_C_COMPILER)
    message(STATUS
        "freertos_demo: no CMAKE_C_COMPILER set; this is a configure-only run. "
        "Pass -DCMAKE_TOOLCHAIN_FILE=../runtime/toolchain.cmake to build.")
    return()
endif()

enable_language(C)
enable_language(ASM)

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(REPO_ROOT       "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(RUNTIME_ROOT    "${REPO_ROOT}/runtime")
set(KERNEL_ROOT     "${REPO_ROOT}/FreeRTOS-Kernel")
set(KERNEL_PORTABLE "${KERNEL_ROOT}/portable/GCC/RISC-V")
set(KERNEL_CHIP_EXT "${KERNEL_PORTABLE}/chip_specific_extensions/RISCV_MTIME_CLINT_no_extensions")
set(GENERATED_PORT_DIR "${CMAKE_CURRENT_BINARY_DIR}/freertos_riscv_port")
file(MAKE_DIRECTORY ${GENERATED_PORT_DIR})

# The upstream RISC-V port contains assembler macros that must keep one
# instruction per line. If a developer-side formatter touches the submodule,
# the macros can become unbuildable. Build this demo from the submodule's
# checked-in portASM/portContext pair without modifying the working tree.
execute_process(
    COMMAND git -C ${KERNEL_ROOT} show HEAD:portable/GCC/RISC-V/portASM.S
    OUTPUT_FILE ${GENERATED_PORT_DIR}/portASM.S
    RESULT_VARIABLE _PORTASM_RC
)
execute_process(
    COMMAND git -C ${KERNEL_ROOT} show HEAD:portable/GCC/RISC-V/portContext.h
    OUTPUT_FILE ${GENERATED_PORT_DIR}/portContext.h
    RESULT_VARIABLE _PORTCONTEXT_RC
)
if(NOT _PORTASM_RC EQUAL 0 OR NOT _PORTCONTEXT_RC EQUAL 0)
    message(FATAL_ERROR "freertos_demo: failed to materialise clean FreeRTOS RISC-V port files")
endif()

# Build the RV32 portion of the runtime directly, bypassing runtime/'s own
# CMakeLists.txt — that file unconditionally adds host_tests/, which tries
# to cross-compile host-libc programs and fails.  Inlining is short enough.
# HAL set updated 2026-05-25 for the TUI pivot: keypad + framebuffer HALs
# removed; user I/O is UART-only.
add_library(runtime_rt STATIC
    ${RUNTIME_ROOT}/src/crt/start.S
    ${RUNTIME_ROOT}/src/hal/halt.c
    ${RUNTIME_ROOT}/src/hal/uart.c
    ${RUNTIME_ROOT}/src/stdlib/string.c
    ${RUNTIME_ROOT}/src/stdlib/stdio.c
    ${RUNTIME_ROOT}/src/stdlib/stdlib.c
    ${RUNTIME_ROOT}/src/stdlib/assert.c
)
target_include_directories(runtime_rt PUBLIC ${RUNTIME_ROOT}/include)
target_compile_options(runtime_rt PRIVATE
    -march=rv32i_zicsr -mabi=ilp32
    -ffreestanding -fno-builtin -O2
    -Wall -Wno-unused-parameter
)

# -----------------------------------------------------------------------------
# FreeRTOS kernel + RISC-V port — built as our own static library so we don't
# have to coax the upstream CMakeLists.txt into our cross environment.
# -----------------------------------------------------------------------------
add_library(freertos_kernel STATIC
    ${KERNEL_ROOT}/list.c
    ${KERNEL_ROOT}/queue.c
    ${KERNEL_ROOT}/tasks.c
    ${KERNEL_ROOT}/timers.c
    ${KERNEL_ROOT}/portable/MemMang/heap_4.c
    ${KERNEL_PORTABLE}/port.c
    ${GENERATED_PORT_DIR}/portASM.S
)

target_include_directories(freertos_kernel PUBLIC
    ${KERNEL_ROOT}/include
    ${GENERATED_PORT_DIR}
    ${KERNEL_PORTABLE}
    ${KERNEL_CHIP_EXT}
    ${CMAKE_CURRENT_SOURCE_DIR}/include   # FreeRTOSConfig.h lives here
)

# portASM.S uses `#include "portContext.h"` which in turn includes the
# chip-extension header; both must be on the GAS preprocessor include path.
# CMake passes target_include_directories through to ASM too, but the .S
# extension already enables `cpp`, so this is just for completeness.
target_compile_options(freertos_kernel PRIVATE
    -march=rv32i_zicsr
    -mabi=ilp32
    -ffreestanding
    -fno-builtin
    -O2
    -Wall
    -Wno-unused-parameter
)

# -----------------------------------------------------------------------------
# Application + port glue
# -----------------------------------------------------------------------------
add_executable(freertos_demo
    app/main.c
    port_glue/hooks.c
    port_glue/app_irq_handler.c
)

target_compile_options(freertos_demo PRIVATE
    -march=rv32i_zicsr
    -mabi=ilp32
    -ffreestanding
    -fno-builtin
    -O2
    -Wall
    -Wextra
)

# libgcc supplies __mulsi3/__udivsi3/__umodsi3 — RV32I has no M extension so
# the compiler emits calls to these helpers for general C arithmetic.  With
# -nostdlib, GCC doesn't auto-link libgcc, so resolve its absolute path and
# link the archive directly.
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -march=rv32i_zicsr -mabi=ilp32
            -print-libgcc-file-name
    OUTPUT_VARIABLE RV_LIBGCC
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "freertos_demo: libgcc = ${RV_LIBGCC}")

target_link_libraries(freertos_demo PRIVATE freertos_kernel runtime_rt ${RV_LIBGCC})

target_link_options(freertos_demo PRIVATE
    -nostdlib
    -nostartfiles
    -static
    -T${RUNTIME_ROOT}/link.ld
)

# Force-include FreeRTOSConfig.h via the kernel target's PUBLIC includes;
# already covered above.

# -----------------------------------------------------------------------------
# Post-link artifacts (disassembly + Verilog hex).  Derive tool paths from
# the toolchain's compiler dir so they always exist.
# -----------------------------------------------------------------------------
get_filename_component(RV_TOOL_DIR ${CMAKE_C_COMPILER} DIRECTORY)
set(RV_OBJDUMP "${RV_TOOL_DIR}/riscv64-unknown-elf-objdump")
set(RV_OBJCOPY "${RV_TOOL_DIR}/riscv64-unknown-elf-objcopy")

add_custom_command(TARGET freertos_demo POST_BUILD
    COMMAND ${RV_OBJDUMP} -d "$<TARGET_FILE:freertos_demo>" > "$<TARGET_FILE:freertos_demo>.disasm"
    COMMENT "Disassembling freertos_demo"
    VERBATIM
)
add_custom_command(TARGET freertos_demo POST_BUILD
    COMMAND ${RV_OBJCOPY} -S -O verilog "$<TARGET_FILE:freertos_demo>" "$<TARGET_FILE:freertos_demo>.hex"
    COMMENT "Generating Verilog hex"
    VERBATIM
)
