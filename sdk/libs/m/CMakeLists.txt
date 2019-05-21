cmake_minimum_required(VERSION 3.1)
project(libm)

file(GLOB_RECURSE SRCS "*.c" "*.cpp" "*.s")
add_library(m STATIC ${SRCS})


target_compile_definitions (m
    PRIVATE
        _NO_POSIX_SPAWN
        _GNU_SOURCE
        _I386MACH_NEED_SOTYPE_FUNCTION
        MISSING_SYSCALL_NAMES
        INTERNAL_NEWLIB
        HAVE_FCNTL
        HAVE_SYSTEM
)

target_compile_options (m
    PRIVATE
        -fpic
        -fno-builtin
)

target_include_directories (m
    PRIVATE
        common
        complex
)

target_link_libraries(m c)

install (
    TARGETS m 
    ARCHIVE DESTINATION ${TOOLCHAIN_PREFIX}/lib
)