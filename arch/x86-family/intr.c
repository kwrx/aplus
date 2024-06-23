/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/syscall.h>

#include <arch/x86/apic.h>
#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>
#include <arch/x86/vmm.h>


extern struct {
        union {
                struct {
                        void (*handler)(void*, uint8_t);
                        spinlock_t lock __packed;
                };

                long __padding[4];
        };
} bootstrap_irq[224];


void* x86_exception_handler(interrupt_frame_t* frame) {

    DEBUG_ASSERT((frame));

    // #if DEBUG_LEVEL_TRACE

    //     kprintf("x86-intr: intno(%p) errno(%p) ip(%p) cs(%p) flags(%p) sp(%p) ss(%p)\n",
    //         frame->intno,
    //         frame->errno,
    //         frame->ip,
    //         frame->cs,
    //         frame->flags,
    //         frame->sp,
    //         frame->ss
    //     );

    // #endif


    if (likely(frame->intno >= 0x20)) {

        current_cpu->frame = frame;
    }



    switch (frame->intno) {

        case 0xFF:
            kpanicf("x86-intr: PANIC! Spourius Interrupt on cpu #%ld\n", arch_cpu_get_current_id());
            break;


        case 0xFE:

#if defined(__x86_64__)
            frame->ax = syscall_invoke(frame->ax, frame->di, frame->si, frame->dx, frame->r10, frame->r8, frame->r9);
#elif defined(__i386__)
            frame->ax = syscall_invoke(frame->ax, frame->bx, frame->cx, frame->dx, frame->si, frame->di, 0);
#endif

            break;


        case 0x21 ... 0xFD:

            if (likely(bootstrap_irq[frame->intno - 0x20].handler))
                bootstrap_irq[frame->intno - 0x20].handler(frame, frame->intno - 0x20);

            else {
#if DEBUG_LEVEL_WARN
                kprintf("x86-intr: WARN! unhandled IRQ #%ld caught, ignoring\n", frame->intno - 0x20);
#endif
            }


            apic_eoi();
            break;


        case 0x20:

        {
            if (unlikely(current_cpu->uptime.tv_nsec + 10000000 > 999999999)) {
                current_cpu->uptime.tv_sec += 1;
                current_cpu->uptime.tv_nsec = 0;
            } else {
                current_cpu->uptime.tv_nsec += 10000000;
            }

            schedule(0);
        }

            apic_eoi();
            break;


        case 0x02:

            // TODO: Handle NMI Interrupts
            kpanicf("x86-nmi: PANIC! exception(%ld), errno(0x%lX), cs(0x%lX), ip(0x%lX), sp(0x%lX), bp(0x%lX), cpu(%ld)\n", frame->intno, frame->errno, frame->cs, frame->ip, frame->sp, frame->bp, current_cpu->id);
            break;

        case 0x0E:

            pagefault_handle(frame, x86_get_cr2());
            break;

        default:

            // TODO: Handle User Exception
            kpanicf("x86-intr: PANIC! exception(%ld), errno(0x%lX), cs(0x%lX), ip(0x%lX), sp(0x%lX), bp(0x%lX), cpu(%ld)\n", frame->intno, frame->errno, frame->cs, frame->ip, frame->sp, frame->bp, current_cpu->id);
            break;
    }



    if (unlikely(current_task->flags & TASK_FLAGS_NEED_RESCHED)) {

        current_task->flags &= ~TASK_FLAGS_NEED_RESCHED;
        schedule(1);
    }

    if (unlikely(current_task->flags & TASK_FLAGS_NEED_SYSCALL_RESTART)) {

        current_task->flags &= ~TASK_FLAGS_NEED_SYSCALL_RESTART;
        frame->ax = syscall_restart();
    }


    return frame;
}



void arch_intr_enable(long s) {

    if (likely(s)) {
        __asm__ __volatile__("sti");
    }
}


long arch_intr_disable(void) {


    long s;

    __asm__ __volatile__(
#if defined(__x86_64__)
        "pushfq         \n"
        "pop %%rax      \n"
#elif defined(__i386__)
        "pushf          \n"
        "pop %%eax      \n"
#endif
        "cli            \n"

        : "=a"(s)::"memory");

    return !!(s & (1 << 9));
}



void arch_intr_map_irq(irq_t irq, void (*handler)(void*, irq_t)) {

    DEBUG_ASSERT(irq < (0xFF - 0x20));
    DEBUG_ASSERT(handler);


    if (unlikely(bootstrap_irq[irq].handler && bootstrap_irq[irq].handler != handler))
        kpanicf("x86-intr: PANIC! can not map irq(%d), already owned by %p\n", irq, bootstrap_irq[irq].handler);


    bootstrap_irq[irq].handler = handler;

    ioapic_map_irq(irq, irq, current_cpu->id);



#if DEBUG_LEVEL_TRACE
    kprintf("x86-intr: map irq(%d) at %p\n", irq, handler);
#endif
}


void arch_intr_unmap_irq(irq_t irq) {

    DEBUG_ASSERT(irq < (0xFF - 0x20));
    DEBUG_ASSERT(bootstrap_irq[irq].handler != NULL);


    bootstrap_irq[irq].handler = NULL;

    ioapic_unmap_irq(irq);


#if DEBUG_LEVEL_TRACE
    kprintf("x86-intr: unmap irq(%d)\n", irq);
#endif
}


void arch_intr_map_irq_without_ioapic(irq_t irq, void (*handler)(void*, irq_t)) {

    DEBUG_ASSERT(irq < (0xFF - 0x20));
    DEBUG_ASSERT(handler);


    if (unlikely(bootstrap_irq[irq].handler && bootstrap_irq[irq].handler != handler))
        kpanicf("x86-intr: PANIC! can not map irq(%d), already owned by %p\n", irq, bootstrap_irq[irq].handler);


    bootstrap_irq[irq].handler = handler;


#if DEBUG_LEVEL_TRACE
    kprintf("x86-intr: map irq(%d) at %p without I/O APIC\n", irq, handler);
#endif
}


void arch_intr_unmap_irq_without_ioapic(irq_t irq) {

    DEBUG_ASSERT(irq < (0xFF - 0x20));
    DEBUG_ASSERT(bootstrap_irq[irq].handler != NULL);


    bootstrap_irq[irq].handler = NULL;


#if DEBUG_LEVEL_TRACE
    kprintf("x86-intr: unmap irq(%d) without I/O APIC\n", irq);
#endif
}
