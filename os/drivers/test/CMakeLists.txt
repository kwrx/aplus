cmake_minimum_required(VERSION 3.1)

file(GLOB_RECURSE SRCS "*.c" "*.cpp" "*.s")
add_executable(test ${SRCS})

set_target_properties(test
    PROPERTIES
        OUTPUT_NAME "test.ko"
)

target_compile_definitions(test
    PRIVATE
        APLUS
        KERNEL
        MODULE
)

target_compile_options(test
    PRIVATE
        -std=gnu11
        -nostdlib
        -include ${CMAKE_BINARY_DIR}/os/arch/config.h
        ${TARGET_KERN_COMPILE_OPTS}
)

target_link_options(test
    PRIVATE
        ${TARGET_KERN_LINK_OPTS}
        -nostdlib
        -r
)

target_include_directories(test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/sdk/toolchain/include
        ${CMAKE_SOURCE_DIR}/os/include
)

install (
    TARGETS test
    RUNTIME DESTINATION ${CMAKE_SOURCE_DIR}/root/usr/lib/modules
)