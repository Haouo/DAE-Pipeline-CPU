cmake_minimum_required(VERSION 3.21...3.28)
project(snake_soc_elf_loader
    VERSION     0.1.0
    DESCRIPTION "ELF loader (Model B) — writes PT_LOAD segments into a MemoryMap"
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

include(CTest)

# --- Dependency: devices/common (MemoryMap, axi_resp_e) ------------------
if(NOT TARGET devices_common)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../devices
                     ${CMAKE_BINARY_DIR}/_devices)
endif()

# --- libelf_loader -------------------------------------------------------
add_library(elf_loader STATIC
    src/elf_loader.c
)
target_include_directories(elf_loader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(elf_loader PUBLIC devices_common devices_dram)
target_compile_options(elf_loader PRIVATE -Wall -Wextra -Wpedantic)

# --- Tests ---------------------------------------------------------------
if(BUILD_TESTING)
    add_executable(test_elf_loader tests/test_elf_loader.c)
    target_link_libraries(test_elf_loader PRIVATE elf_loader devices_dram)
    target_compile_options(test_elf_loader PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ElfLoader.AllUnitTests COMMAND test_elf_loader)
endif()
