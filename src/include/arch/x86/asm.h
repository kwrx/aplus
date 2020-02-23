#ifndef _APLUS_X86_ASM_H
#define _APLUS_X86_ASM_H


#define KERNEL_CS               0x08
#define KERNEL_DS               0x10
#define KERNEL_TSS              0x28


#if defined(__x86_64__)
#   define KERNEL_HIGH_AREA        0xFFFFFFFF80000000
#   define KERNEL_STACK_AREA       0xFFFFFFFFC0000000
#   define KERNEL_STACK_SIZE       0x0000000000200000   //? 2Mib
#   define KERNEL_HEAP_AREA        0xFFFF800000000000
#   define KERNEL_HEAP_SIZE        0x0000020000000000   //? 2TiB

#elif defined(__i386__)
#   error "i386: not supported"
#endif

#define V2P(virt)      \
    ((virt) - KERNEL_HIGH_AREA)



#endif