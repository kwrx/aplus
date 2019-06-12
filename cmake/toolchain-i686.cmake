set(CMAKE_SYSTEM_PROCESSOR x86)

set(ARCH_X86_32 1)
set(ARCH_BITS 32)

set(TARGET_ARCH "x86")
set(TARGET_BITS "32")
set(TARGET_PLATFORM "pc")
set(TARGET_LINK "${CMAKE_SOURCE_DIR}/os/arch/${TARGET_ARCH}/${TARGET_BITS}/link.ld")

set(TARGET_ARCH_COMPILE_OPTS -masm=intel)
set(TARGET_ARCH_LINK_OPTS -z max-page-size=0x1000)

set(TARGET_KERN_COMPILE_OPTS )
set(TARGET_KERN_LINK_OPTS -z max-page-size=0x1000)

set(PAGE_SIZE 4096)


message(STATUS "Configure os/arch ...")
configure_file(
    ${CMAKE_SOURCE_DIR}/os/arch/config.h.in 
    ${CMAKE_BINARY_DIR}/os/arch/config.h 
    ESCAPE_QUOTES @ONLY
)


message(STATUS "Generating mp/ap.h ...")
execute_process (
    COMMAND ${CMAKE_SOURCE_DIR}/extra/utils/gen-ap 
        ${CMAKE_SOURCE_DIR}/os/arch/x86/32/mp/ap.h
        ${CMAKE_SOURCE_DIR}/os/arch/x86/32/mp/ap.asm
)
