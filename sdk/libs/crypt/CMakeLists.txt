cmake_minimum_required(VERSION 3.1)
project(libcrypt)

file (
    GLOB_RECURSE 
    SRCS 
        "*.c" 
        "*.cpp" 
        "*.s"
)

add_library(crypt STATIC ${SRCS})

target_compile_options (crypt
    PRIVATE
        -fpic
)

target_link_libraries (crypt c)

install (
    TARGETS crypt 
    ARCHIVE DESTINATION ${TOOLCHAIN_PREFIX}/lib
)