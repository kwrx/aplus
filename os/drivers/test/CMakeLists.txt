cmake_minimum_required(VERSION 3.1)
set(TARGET test)

file(GLOB_RECURSE SRCS "*.c" "*.cpp" "*.s")
add_executable(${TARGET} ${SRCS})

set_target_properties(${TARGET}
    PROPERTIES
        OUTPUT_NAME "${TARGET}.ko"
)

target_compile_definitions(${TARGET}
    PRIVATE
        APLUS
        KERNEL
        MODULE
)

target_compile_options(${TARGET}
    PRIVATE
        -std=gnu11
        -nostdlib
        -include ${CMAKE_BINARY_DIR}/os/arch/config.h
        ${TARGET_KERN_COMPILE_OPTS}
)

target_link_options(${TARGET}
    PRIVATE
        ${TARGET_KERN_LINK_OPTS}
        -nostdlib
        -r
)

target_include_directories(${TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/sdk/toolchain/include
        ${CMAKE_SOURCE_DIR}/os/include
)

install (
    TARGETS ${TARGET}
    RUNTIME DESTINATION ${CMAKE_SOURCE_DIR}/root/usr/lib/modules
)

add_custom_command (
    TARGET ${TARGET}
    POST_BUILD
    COMMAND ${OBJCOPY} --strip-debug $<TARGET_FILE:${TARGET}>
)