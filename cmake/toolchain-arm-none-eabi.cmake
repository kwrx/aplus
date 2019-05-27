set(CMAKE_SYSTEM_PROCESSOR arm)

set(ARCH_ARM 1)
set(ARCH_BITS 32)

set(TARGET_ARCH "arm")
set(TARGET_BITS "32")
set(TARGET_CPU "v7")
set(TARGET_LINK "${CMAKE_SOURCE_DIR}/os/arch/${TARGET_ARCH}/${TARGET_CPU}/link.ld")
set(TARGET_ARCH_COMPILE_OPTS "-march=armv7-a -mcpu=cortex-a15")
set(TARGET_ARCH_LINK_OPTS "")

set(PAGE_SIZE 4096)


message(STATUS "Configure os/arch ...")
configure_file(
    ${CMAKE_SOURCE_DIR}/os/arch/config.h.in 
    ${CMAKE_BINARY_DIR}/os/arch/config.h 
    ESCAPE_QUOTES @ONLY
)
