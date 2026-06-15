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

#include <signal.h>
#include <stdatomic.h>
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


#define X86_IRQ_COUNT (0xFE - 0x20)

typedef void (*irq_handler_t)(void*, irq_t);

static struct {

    _Atomic(irq_handler_t) handler;
    spinlock_t lock;
    size_t users;
    int type;
    int initialized;

} irq_table[X86_IRQ_COUNT];

static spinlock_t irq_table_lock = SPINLOCK_INIT_WITH_FLAGS(SPINLOCK_FLAGS_CPU_OWNER);


static int x86_exception_signal(interrupt_frame_t* frame, uintptr_t address) {

    siginfo_t info = {0};

    info.si_code = SI_KERNEL;
    info.si_addr = (void*)address;

    switch (frame->intno) {
        case 0x00:
            info.si_signo = SIGFPE;
            info.si_code  = FPE_INTDIV;
            break;
        case 0x01:
        case 0x03:
            info.si_signo = SIGTRAP;
            break;
        case 0x04:
            info.si_signo = SIGFPE;
            info.si_code  = FPE_INTOVF;
            break;
        case 0x05:
            info.si_signo = SIGSEGV;
            info.si_code  = SEGV_BNDERR;
            break;
        case 0x06:
            info.si_signo = SIGILL;
            info.si_code  = ILL_ILLOPC;
            break;
        case 0x07:
            info.si_signo = SIGILL;
            info.si_code  = ILL_COPROC;
            break;
        case 0x0E:
            info.si_signo = SIGSEGV;
            info.si_code  = (frame->errno & X86_PF_P) ? SEGV_ACCERR : SEGV_MAPERR;
            break;
        case 0x10:
        case 0x13:
            info.si_signo = SIGFPE;
            info.si_code  = FPE_FLTINV;
            break;
        case 0x11:
            info.si_signo = SIGBUS;
            info.si_code  = BUS_ADRALN;
            break;
        default:
            info.si_signo = SIGSEGV;
            break;
    }

    if (sched_sigqueueinfo(-1, current_task->pid, current_task->tid, info.si_signo, &info) < 0)
        return -1;

    thread_restart_sched(current_task);
    return 0;
}

static void x86_irq_dispatch(interrupt_frame_t* frame) {

    irq_t irq = frame->intno - 0x20;
    irq_handler_t handler = atomic_load_explicit(&irq_table[irq].handler, memory_order_acquire);

    if (likely(handler)) {
        scoped_lock(&irq_table[irq].lock) {
            handler = atomic_load_explicit(&irq_table[irq].handler, memory_order_relaxed);

            if (likely(handler))
                handler(frame, irq);
        }
    } else {
#if DEBUG_LEVEL_WARN
        kprintf("x86-intr: WARN! unhandled IRQ #%d caught, ignoring\n", irq);
#endif
    }
}


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


    current_cpu->frame = frame;



    switch (frame->intno) {

        case 0xFF:
            return frame;


        case 0xFE:

#if defined(__x86_64__)
            frame->ax = syscall_invoke(frame->ax, frame->di, frame->si, frame->dx, frame->r10, frame->r8, frame->r9);
#elif defined(__i386__)
            frame->ax = syscall_invoke(frame->ax, frame->bx, frame->cx, frame->dx, frame->si, frame->di, 0);
#endif

            break;


        case 0x21 ... 0xFD:

            x86_irq_dispatch(frame);
            apic_eoi();
            break;


        case 0x20:

        {
            current_cpu->uptime.tv_nsec += TASK_SCHEDULER_PERIOD_NS;

            if (unlikely(current_cpu->uptime.tv_nsec >= 1000000000)) {
                current_cpu->uptime.tv_sec += 1;
                current_cpu->uptime.tv_nsec -= 1000000000;
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

            if (pagefault_handle(frame, x86_get_cr2()) == 0)
                break;

            if ((frame->cs & 3) == 3 && x86_exception_signal(frame, x86_get_cr2()) == 0)
                break;

            kpanicf("x86-pfe: PANIC! unhandled page fault, errno(0x%lX), cs(0x%lX), ip(0x%lX), sp(0x%lX), cpu(%ld)\n", frame->errno, frame->cs, frame->ip, frame->sp, current_cpu->id);
            break;

        default:

            if ((frame->cs & 3) == 3 && x86_exception_signal(frame, frame->ip) == 0)
                break;

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



void arch_intr_map_irq(irq_t irq, void (*handler)(void*, irq_t), int type) {

    PANIC_ASSERT(irq > 0 && irq < X86_IRQ_COUNT);
    PANIC_ASSERT(handler);
    PANIC_ASSERT(type == ARCH_INTR_TYPE_DEFAULT || type == ARCH_INTR_TYPE_PCI || type == ARCH_INTR_TYPE_MSI);

    scoped_lock(&irq_table_lock) {
        if (!irq_table[irq].initialized) {
            spinlock_init_with_flags(&irq_table[irq].lock, SPINLOCK_FLAGS_CPU_OWNER);
            irq_table[irq].initialized = 1;
        }

        scoped_lock(&irq_table[irq].lock) {
            irq_handler_t current = atomic_load_explicit(&irq_table[irq].handler, memory_order_relaxed);

            if (unlikely(current && (current != handler || irq_table[irq].type != type)))
                kpanicf("x86-intr: PANIC! can not map irq(%d), already owned by %p\n", irq, current);

            if (irq_table[irq].users++ == 0) {
                irq_table[irq].type = type;
                atomic_store_explicit(&irq_table[irq].handler, handler, memory_order_release);

                switch (type) {
                    case ARCH_INTR_TYPE_DEFAULT: {
                        uint32_t gsi;
                        uint64_t flags;

                        if (irq < 16) {
                            apic_get_isa_irq(irq, &gsi, &flags);
                        } else {
                            gsi   = irq;
                            flags = X86_IOAPIC_REDTTBL_FLAG_TRIGGER_MODE_EDGE | X86_IOAPIC_REDTTBL_FLAG_POLARITY_ACTIVE_HIGH;
                        }

                        ioapic_map_irq(gsi, irq, current_cpu->id, flags);
                        break;
                    }

                    case ARCH_INTR_TYPE_PCI:
                        ioapic_map_irq(irq, irq, current_cpu->id, X86_IOAPIC_REDTTBL_FLAG_TRIGGER_MODE_LEVEL | X86_IOAPIC_REDTTBL_FLAG_POLARITY_ACTIVE_LOW);
                        break;

                    case ARCH_INTR_TYPE_MSI:
                        break;
                }
            }
        }
    }

#if DEBUG_LEVEL_TRACE
    kprintf("x86-intr: map irq(%d) at %p\n", irq, handler);
#endif
}

void arch_intr_unmap_irq(irq_t irq, int type) {

    PANIC_ASSERT(irq > 0 && irq < X86_IRQ_COUNT);
    PANIC_ASSERT(type == ARCH_INTR_TYPE_DEFAULT || type == ARCH_INTR_TYPE_PCI || type == ARCH_INTR_TYPE_MSI);

    scoped_lock(&irq_table_lock) {
        PANIC_ASSERT(irq_table[irq].initialized);

        scoped_lock(&irq_table[irq].lock) {
            PANIC_ASSERT(irq_table[irq].users);
            PANIC_ASSERT(irq_table[irq].type == type);

            if (--irq_table[irq].users == 0) {
                switch (type) {
                    case ARCH_INTR_TYPE_DEFAULT: {
                        uint32_t gsi = irq;

                        if (irq < 16)
                            apic_get_isa_irq(irq, &gsi, NULL);

                        ioapic_unmap_irq(gsi);
                        break;
                    }

                    case ARCH_INTR_TYPE_PCI:
                        ioapic_unmap_irq(irq);
                        break;

                    case ARCH_INTR_TYPE_MSI:
                        break;
                }

                atomic_store_explicit(&irq_table[irq].handler, NULL, memory_order_release);
                irq_table[irq].type = 0;
            }
        }
    }


#if DEBUG_LEVEL_TRACE
    kprintf("x86-intr: unmap irq(%d)\n", irq);
#endif
}
