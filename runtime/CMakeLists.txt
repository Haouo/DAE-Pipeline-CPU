# Snake SoC bare-metal runtime.
#
# Two build modes:
#   1. RV32I target build — produces libruntime.a (HAL + stdlib + crt0). Only
#      activated when a riscv cross compiler is available and the user has not
#      explicitly disabled it. The toolchain is riscv64-unknown-elf-* multilib
#      with -march=rv32i_zicsr -mabi=ilp32 selecting the RV32 variant.
#   2. Host build — always active; compiles only the hardware-independent
#      source files into a small library that host_tests links against. This
#      is how the HAL ⊂ Runtime split is enforced and verified in CI.

cmake_minimum_required(VERSION 3.21...3.28)

# Detect cross toolchain before the project() call so we can apply the
# toolchain file in time. CMake re-invokes itself when the toolchain changes,
# so this only runs cleanly on the first configure pass.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND NOT DEFINED RUNTIME_HOST_ONLY)
    find_program(RISCV_GCC
        NAMES riscv64-unknown-elf-gcc riscv32-unknown-elf-gcc
        HINTS /opt/riscv/bin)
    if(RISCV_GCC)
        message(STATUS "runtime: found ${RISCV_GCC}; enabling RV32 target build")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/toolchain.cmake"
            CACHE FILEPATH "RV32 cross toolchain")
    else()
        message(STATUS "runtime: riscv cross compiler NOT found; "
                       "skipping RV32 target build (host_tests will still run)")
        set(RUNTIME_HOST_ONLY ON CACHE BOOL "Build host tests only" FORCE)
    endif()
endif()

project(runtime LANGUAGES C ASM)

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        OFF)

set(RUNTIME_HW_INDEP_SRC
    src/stdlib/string.c
    src/stdlib/stdio.c
    src/stdlib/stdlib.c
    src/stdlib/assert.c
)

set(RUNTIME_HAL_SRC
    src/hal/halt.c
    src/hal/uart.c
)

set(RUNTIME_CRT_SRC
    src/crt/start.S
)

set(RUNTIME_COMMON_WARNINGS
    -Wall -Wextra -Wpedantic
    -Wshadow -Wstrict-prototypes -Wmissing-prototypes
    -Wnull-dereference -Wpointer-arith
)

if(NOT RUNTIME_HOST_ONLY)
    # --- RV32 target build -------------------------------------------------
    add_library(runtime STATIC
        ${RUNTIME_HW_INDEP_SRC}
        ${RUNTIME_HAL_SRC}
        ${RUNTIME_CRT_SRC}
    )
    target_include_directories(runtime PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )
    target_compile_options(runtime PRIVATE ${RUNTIME_COMMON_WARNINGS})

    # Link script lives next to this CMakeLists; expose it for downstream
    # student programs so they can pass it to their own linker invocations.
    set(RUNTIME_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/link.ld"
        CACHE FILEPATH "Snake SoC Model B link script" FORCE)
endif()

# --- Host tests (always built) --------------------------------------------
enable_testing()
add_subdirectory(host_tests)
