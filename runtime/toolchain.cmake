# Cross-compile toolchain for the Snake SoC bare-metal runtime.
#
# Targets RV32I_Zicsr via the riscv64-unknown-elf-* multilib toolchain shipped
# at /opt/riscv/ (GCC ≥ 16). The riscv64 prefix is the multilib variant; the
# -march / -mabi flags below pick the rv32i/ilp32 subset.
#
# Invocation:
#   cmake -S runtime -B build/runtime -DCMAKE_TOOLCHAIN_FILE=runtime/toolchain.cmake

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)

# Pin to /opt/riscv/bin since the directory is not on PATH by default.
set(_RV_PREFIX /opt/riscv/bin/riscv64-unknown-elf)

set(CMAKE_C_COMPILER   ${_RV_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${_RV_PREFIX}-g++)
set(CMAKE_ASM_COMPILER ${_RV_PREFIX}-gcc)
set(CMAKE_AR           ${_RV_PREFIX}-ar)
set(CMAKE_OBJCOPY      ${_RV_PREFIX}-objcopy)
set(CMAKE_OBJDUMP      ${_RV_PREFIX}-objdump)

# RV32I_Zicsr, soft-float, no compressed extension. Zicsr is required for the
# CSR instructions used by trap handlers (see plans/privileged-arch-plan.md).
set(RV_ARCH_FLAGS "-march=rv32i_zicsr -mabi=ilp32 -mcmodel=medany")

set(CMAKE_C_FLAGS_INIT   "${RV_ARCH_FLAGS} -ffreestanding -fno-builtin -nostdlib")
set(CMAKE_CXX_FLAGS_INIT "${RV_ARCH_FLAGS} -ffreestanding -fno-builtin -nostdlib")
set(CMAKE_ASM_FLAGS_INIT "${RV_ARCH_FLAGS}")

# Bypass the compiler-works probe (it needs a hosted libc we don't have).
set(CMAKE_C_COMPILER_WORKS   1)
set(CMAKE_CXX_COMPILER_WORKS 1)

# Search libraries/headers only from the toolchain; never from host paths.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
