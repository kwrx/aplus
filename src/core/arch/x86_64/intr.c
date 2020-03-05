/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
 *                                                                      
 * This file is part of aPlus.                                          
 *                                                                      
 * aPlus is free software: you can redistribute it and/or modify        
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or    
 * (at your option) any later version.                                  
 *                                                                      
 * aPlus is distributed in the hope that it will be useful,             
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        
 * GNU General Public License for more details.                         
 *                                                                      
 * You should have received a copy of the GNU General Public License    
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.       
 */                                                                     
                                                                        
#include <stdint.h>
#include <string.h>
#include <aplus.h>
#include <aplus/multiboot.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/syscall.h>

#include <hal/cpu.h>
#include <hal/interrupt.h>
#include <hal/vmm.h>

#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>
#include <arch/x86/apic.h>


extern struct {
    union {
        struct {
            void (*handler) (void*);
            spinlock_t lock;
        };

        long __padding[4];
    };
} startup_irq[224];


void x86_exception_handler(interrupt_frame_t* frame) {
    
    DEBUG_ASSERT( (frame));
    DEBUG_ASSERT(!(current_cpu->flags & SMP_CPU_FLAGS_INTERRUPT));



// #if defined(DEBUG) && DEBUG_LEVEL >= 4

//     kprintf("x86-intr: intno(%p) errno(%p) ip(%p) cs(%p) flags(%p) user_sp(%p) user_ss(%p)\n",
//         frame->intno,
//         frame->errno,
//         frame->ip,
//         frame->cs,
//         frame->flags,
//         frame->user_sp,
//         frame->user_ss
//     );

// #endif


    current_cpu->flags |= SMP_CPU_FLAGS_INTERRUPT;


    switch(frame->intno) {

        case 0x20 ... 0xFF:
            break;

        
        case 0x02:

            // TODO: Handle NMI Interrupts
            kpanicf("x86-nmi: PANIC! exception(%p), errno(%p), cs(%p), ip(%p)\n", frame->intno, frame->errno, frame->cs, frame->ip);
            break;

        case 0x0E:

            // TODO: Handle Page Fault (Copy on Write, Swap, ecc...)

            kpanicf("x86-pfe: PANIC! errno(%p), cs(%p), ip(%p), cr2(%p)\n", frame->errno, frame->cs, frame->ip, x86_get_cr2());
            break;

        default:

            // TODO: Handle User Exception

            kpanicf("x86-intr: PANIC! exception(%p), errno(%p), cs(%p), ip(%p)\n", frame->intno, frame->errno, frame->cs, frame->ip);
            break;

    }


    switch(frame->intno) {

        case 0xFF:
            kpanicf("x86-intr: PANIC! Spourius Interrupt on cpu #%d\n", arch_cpu_get_current_id());
            break;

        case 0xFE:

#if defined(__x86_64__)
            frame->ax = syscall_invoke(frame->ax, frame->bx, frame->cx, frame->dx, frame->si, frame->di, frame->r8);
#elif defined(__i386__)
            frame->ax = syscall_invoke(frame->ax, frame->bx, frame->cx, frame->dx, frame->si, frame->di, 0);
#endif

            break;

        case 0x20:

            if(unlikely(current_cpu->ticks.tv_nsec + 1000000 > 999999999)) {
                current_cpu->ticks.tv_sec += 1;
                current_cpu->ticks.tv_nsec = 0;
            } else
                current_cpu->ticks.tv_nsec += 1000000;

            
            schedule(frame);
            
            apic_eoi();
            break;

        case 0x21 ... 0xFD:

            __lock(&startup_irq[frame->intno - 0x20].lock, {

                if(likely(startup_irq[frame->intno - 0x20].handler))
                    startup_irq[frame->intno - 0x20].handler((void*) frame);
                else
                    kprintf("x86-intr: WARN! unhandled IRQ #%d caught, ignoring\n", frame->intno - 0x20);

            });
            
            apic_eoi();
            break;


    }


    current_cpu->flags &= ~SMP_CPU_FLAGS_INTERRUPT;

}



void arch_intr_enable(long s) {

    if(likely(s))
        __asm__ __volatile__ ("sti");

}



long arch_intr_disable(void) {
    

    long s;

    __asm__ __volatile__ (
#if defined(__x86_64__)
        "pushfq         \n"
        "pop %%rax      \n"
#elif defined(__i386__)
        "pushf          \n"
        "pop %%eax      \n"
#endif
        "cli            \n"

        : "=a" (s)
        :: "memory"
    );

    return (s & (1 << 9));

}



void arch_intr_map_irq(uint8_t irq, void (*handler) (void*)) {
    
    DEBUG_ASSERT(irq < (0xFF - 0x20));
    DEBUG_ASSERT(handler);


    __lock(&startup_irq[irq].lock, {

        startup_irq[irq].handler = handler;
        ioapic_map_irq(irq, irq, current_cpu->id);
    
    });


#if defined(DEBUG) && DEBUG_LEVEL >= 1
    kprintf("x86-intr: map irq(%p) at %p\n", irq, handler);
#endif

}


void arch_intr_unmap_irq(uint8_t irq) {

    DEBUG_ASSERT(irq < (0xFF - 0x20));

    __lock(&startup_irq[irq].lock, {
        
        startup_irq[irq].handler = NULL;
        ioapic_map_irq(irq, irq, current_cpu->id);

    });


#if defined(DEBUG) && DEBUG_LEVEL >= 1
    kprintf("x86-intr: unmap irq(%p)\n", irq);
#endif

}