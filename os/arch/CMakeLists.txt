cmake_minimum_required(VERSION 3.1)
project(arch)

file(GLOB SRCS
    "${TARGET_ARCH}/*.c" "${TARGET_ARCH}/*.cpp" "${TARGET_ARCH}/*.asm"
    "${TARGET_ARCH}/${TARGET_BITS}/*.c" "${TARGET_ARCH}/${TARGET_BITS}/*.cpp" "${TARGET_ARCH}/${TARGET_BITS}/*.asm"
)

add_library(arch ${SRCS})

target_compile_definitions(arch
    PRIVATE
        APLUS
        KERNEL
)

target_compile_options (arch
    PRIVATE
        -std=gnu11
        -nostdlib
        -include ${CMAKE_BINARY_DIR}/os/arch/config.h
        ${TARGET_ARCH_COMPILE_OPTS}
)

target_link_options(arch
    PRIVATE
        ${TARGET_ARCH_LINK_OPTS}
)

target_include_directories(arch
    PRIVATE
        ${CMAKE_SOURCE_DIR}/sdk/toolchain/include
        ${CMAKE_SOURCE_DIR}/os/include
)

target_link_libraries(arch
    vterm 
    aplus
    c
    m
    ssp
    gcc
)