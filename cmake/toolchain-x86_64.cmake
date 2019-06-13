set(CMAKE_SYSTEM_PROCESSOR x86)

set(ARCH_X86_64 1)
set(ARCH_BITS 64)

set(TARGET_ARCH "x86")
set(TARGET_BITS "64")
set(TARGET_PLATFORM "pc")
set(TARGET_LINK "${CMAKE_SOURCE_DIR}/os/arch/${TARGET_ARCH}/${TARGET_BITS}/link.ld")

# /os/arch
set(TARGET_ARCH_COMPILE_OPTS -m64 -mcmodel=kernel -mno-red-zone -masm=intel)
set(TARGET_ARCH_LINK_OPTS -z max-page-size=0x1000)

# /os/kernel
set(TARGET_KERN_COMPILE_OPTS -m64 -mcmodel=kernel -mno-red-zone)
set(TARGET_KERN_LINK_OPTS -z max-page-size=0x1000)

# /os/drivers
set(TARGET_MODS_COMPILE_OPTS -m64 -mcmodel=large -mno-red-zone)
set(TARGET_MODS_LINK_OPTS -z max-page-size=0x1000)

# /os/drivers/platform
set(TARGET_MODS_PLAT_COMPILE_OPTS -m64 -mcmodel=large -mno-red-zone -masm=intel)
set(TARGET_MODS_PLAT_LINK_OPTS -z max-page-size=0x1000)


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
        ${CMAKE_SOURCE_DIR}/os/arch/x86/64/mp/ap.h
        ${CMAKE_SOURCE_DIR}/os/arch/x86/64/mp/ap.asm
)
