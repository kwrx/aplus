cmake_minimum_required(VERSION 3.1)
project(libpthread)

file(GLOB_RECURSE SRCS "*.c" "*.cpp" "*.s")
add_library(pthread STATIC ${SRCS})

target_compile_options (pthread
    PRIVATE
        -fpic
)

target_link_libraries (pthread c)

install (
    TARGETS pthread 
    ARCHIVE DESTINATION ${TOOLCHAIN_PREFIX}/lib
)