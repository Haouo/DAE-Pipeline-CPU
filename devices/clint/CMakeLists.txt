cmake_minimum_required(VERSION 3.21...3.28)

# CLINT device — MmioDevice + Tickable, no external deps.

add_library(devices_clint STATIC
    src/clint.c
)
target_include_directories(devices_clint PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(devices_clint
    PUBLIC devices_common
)
target_compile_features(devices_clint PUBLIC c_std_11)
target_compile_options(devices_clint PRIVATE
    -Wall -Wextra -Wpedantic
)

# --- Tests ----------------------------------------------------------------
if(BUILD_TESTING)
    add_executable(test_clint tests/test_clint.c)
    target_link_libraries(test_clint PRIVATE devices_clint)
    target_compile_options(test_clint PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME CLINT.AllUnitTests COMMAND test_clint)
endif()
