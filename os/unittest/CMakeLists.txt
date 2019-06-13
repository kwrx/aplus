cmake_minimum_required(VERSION 3.1)
project(unittest)

if(CMAKE_BUILD_TYPE MATCHES Debug)


    file(GLOB SRCS
        "arch/${TARGET_ARCH}/*.c"
        "arch/${TARGET_ARCH}/*.cpp"
        "arch/${TARGET_ARCH}/*.s"
        "arch/${TARGET_ARCH}/${TARGET_BITS}/*.c"
        "arch/${TARGET_ARCH}/${TARGET_BITS}/*.cpp"
        "arch/${TARGET_ARCH}/${TARGET_BITS}/*.s"
        "kernel/*.c"
        "kernel/*.cpp"
        "kernel/*.s"
        "*.c"
        "*.cpp"
        "*.s"
    )

    add_library(unittest ${SRCS})



    target_compile_definitions(unittest
        PRIVATE
            APLUS
            KERNEL
            UNITTEST
    )

    target_compile_options (unittest
        PRIVATE
            -std=gnu11
            -nostdlib
            -include ${CMAKE_BINARY_DIR}/os/arch/config.h

    )

    target_include_directories(unittest
        PRIVATE
            ${CMAKE_SOURCE_DIR}/sdk/toolchain/include
            ${CMAKE_SOURCE_DIR}/os/include
    )

    target_link_libraries(unittest
        aplus
        c
        m
        ssp
        gcc
    )

    add_custom_command (
        TARGET unittest
        POST_BUILD
        COMMAND ${OBJCOPY} --only-keep-debug $<TARGET_FILE:unittest> $<TARGET_FILE:unittest>.debug
        COMMAND ${OBJCOPY} --strip-debug $<TARGET_FILE:unittest>
    )


endif()