cmake_minimum_required(VERSION 3.1)
project(systemdev)

file (
    GLOB devs
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} 
             ${CMAKE_CURRENT_SOURCE_DIR}/*
)

foreach(dev ${devs})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${dev})
    
        file (
            GLOB_RECURSE sources
                "${dev}/*.c"
                "${dev}/*.cpp"
                "${dev}/*.s"
        )

        add_executable(${dev} ${sources})


        set_target_properties (
            ${dev}
                PROPERTIES
                    OUTPUT_NAME "${dev}.ko"
        )

        target_compile_definitions (
            ${dev}
                PRIVATE
                    APLUS
                    KERNEL
                    MODULE
        )

        target_compile_options (
            ${dev}
                PRIVATE
                    -std=gnu11
                    -nostdlib
                    -include ${CMAKE_BINARY_DIR}/os/arch/config.h
                    ${TARGET_ARCH_COMPILE_OPTS}
        )

        target_link_options (
            ${dev}
                PRIVATE
                    ${TARGET_ARCH_LINK_OPTS}
                    -nostdlib
                    -r
        )

        target_include_directories (
            ${dev}
                PRIVATE
                    ${CMAKE_SOURCE_DIR}/sdk/toolchain/include
                    ${CMAKE_SOURCE_DIR}/os/include
                    ${CMAKE_SOURCE_DIR}/os/drivers/include
        )


        install (
            TARGETS ${dev}
            RUNTIME DESTINATION ${CMAKE_SOURCE_DIR}/root/usr/lib/modules/block
        )

        add_custom_command (
            TARGET ${dev}
            POST_BUILD
            COMMAND ${OBJCOPY} $<TARGET_FILE:${dev}> $<TARGET_FILE:${dev}>.debug
            COMMAND ${OBJCOPY} --strip-debug $<TARGET_FILE:${dev}>
        )
    
    endif()
endforeach(dev ${devs})

