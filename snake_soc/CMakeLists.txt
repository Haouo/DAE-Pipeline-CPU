cmake_minimum_required(VERSION 3.21...3.28)
project(snake_soc
    VERSION     0.1.0
    DESCRIPTION "Snake SoC composition (DAE Pipeline CPU testbench backend)"
    LANGUAGES   C
)

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        ON)   # container_of in devices/common

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING
        "Build type (Debug|Release|RelWithDebInfo|MinSizeRel)" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

# --- Dependencies --------------------------------------------------------
# devices/ provides MemoryMap, Ticker, TerminalBackend, and the 5 concrete
# peripheral devices (BootROM / CLINT / UART / IRQ-AGG / DRAM). elf_loader/
# writes ELF segments into a MemoryMap. The CPU is driven externally by the
# DAE Pipeline RTL via DPI, so the software ISS Core is not built here.
if(NOT TARGET devices_common)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../devices
                     ${CMAKE_BINARY_DIR}/_devices)
endif()
if(NOT TARGET elf_loader)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../elf_loader
                     ${CMAKE_BINARY_DIR}/_elf_loader)
endif()

# --- libsnake_soc --------------------------------------------------------
add_library(snake_soc STATIC
    src/snake_soc.c
)
target_include_directories(snake_soc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(snake_soc PUBLIC
    devices_common
    devices_boot_rom
    devices_uart
    devices_dram
    devices_clint
    devices_irq_agg
    elf_loader
)
target_compile_options(snake_soc PRIVATE -Wall -Wextra -Wpedantic)

message(STATUS "")
message(STATUS "Snake SoC composition (DAE Pipeline CPU testbench backend)")
message(STATUS "  Build type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "  Peripherals:          BootROM / CLINT / UART / IRQ-AGG / DRAM")
message(STATUS "")
