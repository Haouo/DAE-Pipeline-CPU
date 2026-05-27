cmake_minimum_required(VERSION 3.21...3.28)
project(snake_soc_devices
    VERSION     0.1.0
    DESCRIPTION "Snake SoC peripheral devices (MmioDevice + Tickable trait composition)"
    LANGUAGES   C
)

# C standard
set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        ON)   # need GCC __extension__ for container_of

# Default to Debug if not specified — helps surface UB / asserts early.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING
        "Build type (Debug|Release|RelWithDebInfo|MinSizeRel)" FORCE)
endif()

# Export compile commands so clangd and other tools can find headers.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

# --- Testing -------------------------------------------------------------
include(CTest)
# BUILD_TESTING defaults to ON (via include(CTest)); per-device CMakeLists
# check it before adding test targets.

# --- Sub-projects --------------------------------------------------------
# Order matters: common first (provides devices_common + the TerminalBackend
# for raw-mode stdin), then leaf devices. Framebuffer + Keypad were removed
# on 2026-05-25 in the TUI pivot — UART is now the only user-facing I/O.
add_subdirectory(common)
add_subdirectory(boot_rom)
add_subdirectory(uart)
add_subdirectory(dram)
add_subdirectory(clint)
add_subdirectory(irq_agg)

# --- Summary -------------------------------------------------------------
message(STATUS "")
message(STATUS "Snake SoC devices — build summary (5 peripherals post-TUI-pivot 2026-05-25)")
message(STATUS "  Build type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "  Devices common:       devices_common (incl. TerminalBackend)")
message(STATUS "  Peripherals:          BootROM / CLINT / UART (full-duplex) / IRQ-AGG / DRAM")
message(STATUS "  Testing:              ${BUILD_TESTING}")
message(STATUS "")
