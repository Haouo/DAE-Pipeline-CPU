cmake_minimum_required(VERSION 3.21...3.28)
project(dae_pipeline_cpu
    VERSION     0.1.0
    DESCRIPTION "DAE Pipeline CPU — RV32I+Zicsr educational core with Snake SoC DPI testbench"
    LANGUAGES   C
)

# Top-level build for the standalone DAE Pipeline CPU repository.
#
# Builds:
#   - libsnake_soc.a and its device dependencies (C-side backend for the
#     Verilator DPI testbench), via snake_soc/.
#   - Optionally cross-compiles the upstream riscv-tests submodule against
#     riscv-env-custom/snake/ into ${CMAKE_BINARY_DIR}/riscv-tests/, via
#     tests/riscv-tests/. Self-skips if the cross compiler or submodule is
#     missing.
#
# The RTL itself is driven by Verilator from scripts/run-riscv-tests.sh, not
# from CMake. CMake only produces the artefacts that script links against.

add_subdirectory(snake_soc)

option(BUILD_RISCV_TESTS "Cross-compile riscv-tests for the DPI testbench" ON)
if(BUILD_RISCV_TESTS)
    add_subdirectory(tests/riscv-tests)
endif()
