cmake_minimum_required(VERSION 3.21...3.28)

# UART device — MmioDevice + Tickable (full-duplex post-2026-05-25 TUI
# pivot). Depends on devices_common (TerminalBackend for raw-mode stdin)
# and devices_irq_agg (IrqAgg_set_uart_rx_edge for the RX-valid rising-
# edge wire).

add_library(devices_uart STATIC
    src/uart.c
)
target_include_directories(devices_uart PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(devices_uart
    PUBLIC  devices_common
    PRIVATE devices_irq_agg
)
target_compile_features(devices_uart PUBLIC c_std_11)
target_compile_options(devices_uart PRIVATE
    -Wall -Wextra -Wpedantic
)

# --- Tests ----------------------------------------------------------------
if(BUILD_TESTING)
    add_executable(test_uart tests/test_uart.c)
    target_link_libraries(test_uart PRIVATE devices_uart)
    target_compile_options(test_uart PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME UART.AllUnitTests COMMAND test_uart)
endif()
