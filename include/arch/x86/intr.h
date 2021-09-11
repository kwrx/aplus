#ifndef _APLUS_X86_INTR_H
#define _APLUS_X86_INTR_H

#ifndef __ASSEMBLY__

#include <aplus.h>
#include <aplus/debug.h>


//? NOTE: IRQs allocatable from drivers (see MSI-X)
//? @see 10.11.2 - Message Data Register Format
#define IRQ_MSIX_ALLOCATABLE_OFFSET             (0x10)  // 16 or higher


typedef struct {

    uintptr_t di;
    uintptr_t si;
    uintptr_t bp;
    uintptr_t bx;
    uintptr_t dx;
    uintptr_t cx;
    uintptr_t ax;

#if defined(__x86_64__)
    uintptr_t r8;
    uintptr_t r9;
    uintptr_t r10;
    uintptr_t r11;
    uintptr_t r12;
    uintptr_t r13;
    uintptr_t r14;
    uintptr_t r15;
#endif

    uintptr_t intno;
    uintptr_t errno;

    uintptr_t ip;
    uintptr_t cs;
    uintptr_t flags;

    uintptr_t sp;
    uintptr_t ss;
    
} __packed interrupt_frame_t;


typedef struct {

    union {
        
        struct {
            
            void* ustack;
            void* kstack;

            long flags;
            sigset_t mask;
            interrupt_frame_t regs;

        };

        char __padding[512 - 16];
    };

    char fpuregs[0];

} __packed sigcontext_frame_t;



__BEGIN_DECLS

void timer_init();

__END_DECLS

#endif
#endif