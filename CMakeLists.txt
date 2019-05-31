cmake_minimum_required(VERSION 3.1)
project(aplus)

enable_language(C)
enable_language(CXX)
enable_language(ASM)

if(TARGET_ARCH MATCHES "x86")
    
    enable_language(ASM_NASM)
    
    if(TARGET_BITS MATCHES "32")
        set(CMAKE_ASM_NASM_COMPILE_OBJECT 
            "<CMAKE_ASM_NASM_COMPILER> -felf -o <OBJECT> <SOURCE>")
    else()
        set(CMAKE_ASM_NASM_COMPILE_OBJECT 
            "<CMAKE_ASM_NASM_COMPILER> -felf64 -o <OBJECT> <SOURCE>")
    endif()

endif()


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG)
    add_compile_options(-Og)
endif()


add_subdirectory(sdk/libs)
add_subdirectory(os/arch)
add_subdirectory(os/kernel)
add_subdirectory(os/drivers)
add_subdirectory(usr/apps)