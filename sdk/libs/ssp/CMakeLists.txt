cmake_minimum_required(VERSION 3.1)
project(libssp)

file(GLOB_RECURSE SRCS "*.c" "*.cpp" "*.s")
add_library(ssp STATIC ${SRCS})

target_compile_options (ssp
    PRIVATE
        -fpic
)

target_link_libraries (ssp c)

install (
    TARGETS ssp 
    ARCHIVE DESTINATION ${TOOLCHAIN_PREFIX}/lib
)