cmake_minimum_required(VERSION 3.1)
project(libaplus)

file(GLOB_RECURSE SRCS "*.c" "*.cpp" "*.s")
add_library(aplus STATIC ${SRCS})


target_compile_definitions (aplus
    PRIVATE
        NO_CAIRO_EXTENSION
)

target_compile_options (aplus
    PRIVATE
        -fpic
)

target_link_libraries (aplus c)

install (
    TARGETS aplus 
    ARCHIVE DESTINATION ${TOOLCHAIN_PREFIX}/lib
)