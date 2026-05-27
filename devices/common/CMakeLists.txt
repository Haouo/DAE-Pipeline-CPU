cmake_minimum_required(VERSION 3.21...3.28)

# --- Library: devices_common -----------------------------------------------
# Pure C, no external dependencies. Provides the trait interfaces and
# registries (MmioDevice, Tickable, MemoryMap, Ticker) plus the
# TerminalBackend that owns raw-mode stdin + stdout passthrough for the
# UART RX/TX path (replaces the pre-2026-05-25 SDL2 backend). Every device
# library links to this.

add_library(devices_common STATIC
    src/mmio_device.c
    src/tickable.c
    src/mem_map.c
    src/ticker.c
    src/logging_mmio_device.c
    src/terminal_backend.c
)
target_include_directories(devices_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(devices_common PUBLIC c_std_11)
target_compile_options(devices_common PRIVATE
    -Wall -Wextra -Wpedantic
)

# --- Tests for devices_common ---------------------------------------------
if(BUILD_TESTING)
    add_executable(test_logging_mmio_device tests/test_logging_mmio_device.c)
    target_link_libraries(test_logging_mmio_device PRIVATE devices_common)
    target_compile_options(test_logging_mmio_device PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME LoggingMmioDevice.AllUnitTests COMMAND test_logging_mmio_device)
endif()
