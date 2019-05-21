cmake_minimum_required(VERSION 3.1)
project(kernel)

file(GLOB_RECURSE SRCS "*.c" "*.cpp" "*.s")
add_executable(kernel ${ARCH} ${SRCS})



set_target_properties (kernel
    PROPERTIES 
        OUTPUT_NAME "aplus"
)

target_compile_definitions (kernel
    PRIVATE
        APLUS
        KERNEL
)

target_compile_options (kernel
    PRIVATE
        -std=gnu11
        -nostdlib
        -include ${CMAKE_BINARY_DIR}/os/arch/config.h
)

target_link_options(kernel
    PRIVATE
        -Wl,-Map=${CMAKE_SOURCE_DIR}/kernel.map
        -Wl,-T${TARGET_LINK}
        -nostdlib
)

target_include_directories(kernel
    PRIVATE
        ${CMAKE_SOURCE_DIR}/sdk/toolchain/include
        ${CMAKE_SOURCE_DIR}/os/include
)

target_link_libraries(kernel
    arch
    vterm 
    aplus
    c
    m
    ssp
    gcc
)

install (
    TARGETS kernel
    RUNTIME DESTINATION ${CMAKE_SOURCE_DIR}/root
)