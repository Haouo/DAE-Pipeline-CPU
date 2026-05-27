# Host build of the HW-independent runtime modules + their unit tests.
#
# Builds with the host compiler (NOT the RV cross-compiler) so the tests can
# run on the development machine without an RV simulator. Only string.c,
# stdio.c, and stdlib.c are compiled — every HAL symbol they depend on is
# resolved against mock_hal.c, which also provides _sheap / _eheap via
# mock_heap.S so the bump allocator's "linker-style" heap symbols look the
# same on host as on the RV target.

# The host build deliberately uses host gcc, not the parent project's
# (potentially cross-) compiler. We re-export the include path so individual
# tests can pull in <stdlib/...> and <hal/...> headers without absolute paths.

set(HOST_RUNTIME_SRC
    ../src/stdlib/string.c
    ../src/stdlib/stdio.c
    ../src/stdlib/stdlib.c
)

add_library(runtime_stdlib_host STATIC ${HOST_RUNTIME_SRC})
target_include_directories(runtime_stdlib_host PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_compile_options(runtime_stdlib_host PRIVATE
    -Wall -Wextra -Wpedantic
    -Wshadow -Wstrict-prototypes -Wmissing-prototypes
    -Wno-unused-parameter
    # Pin host build to the same standard the RV target uses.
    -std=c11
)
# Host-build flag rewrites the public malloc/free/exit symbols to rt_*. See
# include/stdlib/stdlib.h for the rationale. The flag must be PUBLIC so test
# translation units that include <stdlib/stdlib.h> see the same macros.
target_compile_definitions(runtime_stdlib_host PUBLIC RUNTIME_HOST_BUILD=1)

enable_language(ASM)

add_library(mock_hal STATIC mock_hal.c mock_heap.S)
target_include_directories(mock_hal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

function(add_runtime_host_test name)
    add_executable(${name} ${name}.c)
    # mock_hal references rt_free (defined in runtime_stdlib_host), so list
    # mock_hal first; the linker resolves rt_free from the next archive on
    # the line. Wrap in --start-group/--end-group for belt-and-suspenders so
    # any future cross-references between the two archives keep linking.
    target_link_libraries(${name} PRIVATE
        -Wl,--start-group mock_hal runtime_stdlib_host -Wl,--end-group)
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    add_test(NAME runtime.${name} COMMAND ${name})
endfunction()

add_runtime_host_test(test_string)
add_runtime_host_test(test_printf)
add_runtime_host_test(test_malloc)
