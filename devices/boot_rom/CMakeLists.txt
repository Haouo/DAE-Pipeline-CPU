cmake_minimum_required(VERSION 3.21...3.28)

# BootROM device — MmioDevice; 4 KB read-only with 2-instruction stub.

add_library(devices_boot_rom STATIC
    src/boot_rom.c
)
target_include_directories(devices_boot_rom PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(devices_boot_rom
    PUBLIC devices_common
)
target_compile_features(devices_boot_rom PUBLIC c_std_11)
target_compile_options(devices_boot_rom PRIVATE
    -Wall -Wextra -Wpedantic
)

if(BUILD_TESTING)
    add_executable(test_boot_rom tests/test_boot_rom.c)
    target_link_libraries(test_boot_rom PRIVATE devices_boot_rom)
    target_compile_options(test_boot_rom PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME BootROM.AllUnitTests COMMAND test_boot_rom)
endif()
