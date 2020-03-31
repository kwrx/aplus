#ifndef _APLUS_X86_ASM_H
#define _APLUS_X86_ASM_H


#define KERNEL_CS                   0x08
#define KERNEL_DS                   0x10
#define KERNEL_TSS                  0x28

#define USER_CS                     0x20
#define USER_DS                     0x18

#define KERNEL_TSS_MAX              256


#define AP_BOOT_OFFSET              0x00001000          //? 4KiB
#define AP_BOOT_AP16_OFFSET         0x00000000
#define AP_BOOT_AP32_OFFSET         0x00000100
#define AP_BOOT_AP64_OFFSET         0x00000200
#define AP_BOOT_GDT32_OFFSET        0x00000F00
#define AP_BOOT_GDT32_PTR_OFFSET    0x00000F18
#define AP_BOOT_GDT64_OFFSET        0x00000F20
#define AP_BOOT_GDT64_PTR_OFFSET    0x00000F38
#define AP_BOOT_HEADER_OFFSET       0x00000F40

#define AP_BOOT_HEADER_MAGIC        0xCAFEBABEDEADC0DE


#if defined(__x86_64__)
#   define KERNEL_HIGH_AREA         0xFFFFFFFF80000000
#   define KERNEL_STACK_AREA        0xFFFFFFFFC0000000
#   define KERNEL_STACK_SIZE        0x0000000000200000   //? 2Mib
#   define KERNEL_HEAP_AREA         0xFFFF800000000000
#   define KERNEL_HEAP_SIZE         0x0000020000000000   //? 2TiB

#elif defined(__i386__)
#   error "i386: not supported"
#endif

#define V2P(virt)      \
    ((virt) - KERNEL_HIGH_AREA)



#define KERNEL_INTERRUPT_STACK      KERNEL_STACK_AREA
#define KERNEL_INTERRUPT_STACKSIZE  0x4000
#define KERNEL_EXCEPTION_STACK      KERNEL_STACK_AREA + KERNEL_INTERRUPT_STACKSIZE
#define KERNEL_EXCEPTION_STACKSIZE  0x1000
#define KERNEL_SYSCALL_STACK        KERNEL_EXCEPTION_STACK + KERNEL_EXCEPTION_STACKSIZE
#define KERNEL_SYSCALL_STACKSIZE    0x8000

#endif