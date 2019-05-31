cmake_minimum_required(VERSION 3.1)
project(libc)

file (
    GLOB_RECURSE 
    SRCS 
        "*.c" 
        "*.cpp" 
        "*.s"
)

add_library(c STATIC ${SRCS})


target_compile_definitions (c
    PRIVATE
        _NO_POSIX_SPAWN
        _GNU_SOURCE
        _I386MACH_NEED_SOTYPE_FUNCTION
        MISSING_SYSCALL_NAMES
        INTERNAL_NEWLIB
        HAVE_FCNTL
        HAVE_SYSTEM
)

target_compile_options (c
    PRIVATE
        -fpic
        -fno-builtin
)

install (
    TARGETS c 
    ARCHIVE DESTINATION ${TOOLCHAIN_PREFIX}/lib
)