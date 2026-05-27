cmake_minimum_required(VERSION 3.21...3.28)

# DRAM device — MmioDevice + Tickable; ideal model (Lab 4–6). The Tickable
# trait is registered with the Ticker only when the abstract model is
# selected (Lab 7 deliverable).

add_library(devices_dram STATIC
    src/dram.c
)
target_include_directories(devices_dram PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(devices_dram
    PUBLIC devices_common
)
target_compile_features(devices_dram PUBLIC c_std_11)
target_compile_options(devices_dram PRIVATE
    -Wall -Wextra -Wpedantic
)

if(BUILD_TESTING)
    add_executable(test_dram tests/test_dram.c)
    target_link_libraries(test_dram PRIVATE devices_dram)
    target_compile_options(test_dram PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME DRAM.AllUnitTests COMMAND test_dram)
endif()
