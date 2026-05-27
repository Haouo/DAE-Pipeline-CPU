cmake_minimum_required(VERSION 3.21...3.28)

# IRQ aggregator device — MmioDevice + Tickable, no external deps.

add_library(devices_irq_agg STATIC
    src/irq_agg.c
)
target_include_directories(devices_irq_agg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(devices_irq_agg
    PUBLIC devices_common
)
target_compile_features(devices_irq_agg PUBLIC c_std_11)
target_compile_options(devices_irq_agg PRIVATE
    -Wall -Wextra -Wpedantic
)

# --- Tests ----------------------------------------------------------------
if(BUILD_TESTING)
    add_executable(test_irq_agg tests/test_irq_agg.c)
    target_link_libraries(test_irq_agg PRIVATE devices_irq_agg)
    target_compile_options(test_irq_agg PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME IrqAgg.AllUnitTests COMMAND test_irq_agg)
endif()
