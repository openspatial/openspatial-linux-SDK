set(CMAKE_INCLUDE_CURRENT_DIR ON)

project (test-bench)

SET (CMAKE_C_FLAGS      "")

include(FindGLIB2)

include_directories(${testbed_SOURCE_DIR}/include
                    ${bluez_SOURCE_DIR}/include
                    # for glib wrappers
                    /usr/include/glib-2.0/
                    /usr/lib/x86_64-linux-gnu/glib-2.0/include/
                    )

# Add sources
set(testbench_SOURCES
                test_main.c
)

add_executable(test-bench ${testbench_SOURCES})

target_link_libraries(test-bench
                    nodlabs
                    bluez
                    -lglib-2.0 
)

install(TARGETS test-bench
    RUNTIME DESTINATION bin
)
