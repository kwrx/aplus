cmake_minimum_required(VERSION 3.1)
project(libcrt)

add_library(crt0 OBJECT crt0.c)
add_library(crti OBJECT crti.s)
add_library(crtn OBJECT crtn.s)
add_library(rcrt1 OBJECT rcrt1.c)
add_library(Scrt1 OBJECT Scrt1.c)

target_compile_options(crt0 PRIVATE -fpic)
target_compile_options(crti PRIVATE -fpic)
target_compile_options(crtn PRIVATE -fpic)
target_compile_options(rcrt1 PRIVATE -fpic)
target_compile_options(Scrt1 PRIVATE -fpic)

target_link_libraries (crt0 crti crtn rcrt1 Scrt1 c)

install (FILES $<TARGET_OBJECTS:crt0> DESTINATION ${TOOLCHAIN_PREFIX}/lib RENAME crt0.o)
install (FILES $<TARGET_OBJECTS:crti> DESTINATION ${TOOLCHAIN_PREFIX}/lib RENAME crti.o)
install (FILES $<TARGET_OBJECTS:crtn> DESTINATION ${TOOLCHAIN_PREFIX}/lib RENAME crtn.o)
install (FILES $<TARGET_OBJECTS:rcrt1> DESTINATION ${TOOLCHAIN_PREFIX}/lib RENAME rcrt1.o)
install (FILES $<TARGET_OBJECTS:Scrt1> DESTINATION ${TOOLCHAIN_PREFIX}/lib RENAME Scrt1.o)