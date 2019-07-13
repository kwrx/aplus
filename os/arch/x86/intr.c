/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>
#include <arch/x86/apic.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>


#define SEG_DESCTYPE(x)         ((x) << 0x04)
#define SEG_PRES(x)             ((x) << 0x07)
#define SEG_SAVL(x)             ((x) << 0x0C)
#define SEG_LONG(x)             ((x) << 0x0D)
#define SEG_SIZE(x)             ((x) << 0x0E)
#define SEG_GRAN(x)             ((x) << 0x0F)
#define SEG_PRIV(x)             (((x) & 0x03) << 0x05)
 
#define SEG_DATA_RDWR           0x02
#define SEG_CODE_EXRD           0x0A


#define IDT_ISR_TRAP            0x8F
#define IDT_ISR_INTERRUPT       0x8E


extern struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t unused;
    uint8_t flags;
    uint16_t base_high;
#if defined(ARCH_X86_64)
    uint32_t base_xhigh;
    uint32_t padding;
#endif
} __packed idtp[X86_IDT_MAX];

static struct {
    void* (*handler) (void*);
    spinlock_t lock;
} irqs[X86_IDT_MAX - 32];

extern uintptr_t isrs[X86_IDT_MAX];
extern uint64_t gdtp[X86_GDT_MAX];
extern uintptr_t early_stack;

static tss_t sys_tss;



static void gd_set(int index, uintptr_t base, uintptr_t limit, uint16_t type) {
    DEBUG_ASSERT(index < X86_GDT_MAX);
    DEBUG_ASSERT(index != 0);

    uint8_t* d = (uint8_t*) &gdtp[index];

    if(limit > 0xFFFF)
        limit >>= 12;


    d[0] = limit & 0xFF;
    d[1] = (limit >> 8) & 0xFF;
    d[6] = ((type >> 8) & 0xFF) | ((limit >> 16) & 0x0F);
    
    d[2] = base & 0xFF;
    d[3] = (base >> 8) & 0xFF;
    d[4] = (base >> 16) & 0xFF;
    d[7] = (base >> 24) & 0xFF;
    
    d[5] = type & 0xFF;
}



static void id_set(int index, uintptr_t isr, uint8_t type) {
#if defined(ARCH_X86_64)
    idtp[index].base_low = (isr) & 0xFFFF;
    idtp[index].base_high = (isr >> 16) & 0xFFFF;
    idtp[index].base_xhigh = (isr >> 32) & 0xFFFFFFFF;
    idtp[index].selector = 0x08;
    idtp[index].unused = 0;
    idtp[index].padding = 0;
    idtp[index].flags = type;
#else
    idtp[index].base_low = (isr) & 0xFFFF;
    idtp[index].base_high = (isr >> 16) & 0xFFFF;
    idtp[index].selector = 0x08;
    idtp[index].unused = 0;
    idtp[index].flags = type;
#endif
}



void intr_init(void) {

#if defined(ARCH_X86_64)

    gd_set(1, 0, 0, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_PRIV(0) | SEG_LONG(1) | SEG_CODE_EXRD);
    gd_set(2, 0, 0, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_PRIV(0) | SEG_LONG(1) | SEG_DATA_RDWR);
    gd_set(3, 0, 0, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_PRIV(3) | SEG_LONG(1) | SEG_CODE_EXRD);
    gd_set(4, 0, 0, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_PRIV(3) | SEG_LONG(1) | SEG_DATA_RDWR);
    gd_set(5, (uintptr_t) &sys_tss & 0xFFFFFFFF, sizeof(tss_t), 0x89);
    gdtp[6] = ((uintptr_t) &sys_tss >> 32) & 0xFFFFFFFF;
    
#else

    memset(&sys_tss, 0, sizeof(tss_t));
    sys_tss.esp = (uintptr_t) &early_stack;
    sys_tss.ss = 0x10;
    sys_tss.cs = 0x08;
    sys_tss.iopb = sizeof(sys_tss);

    gd_set(1, 0, 0xFFFFFFFF, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | SEG_LONG(0) | SEG_SIZE(1) | SEG_GRAN(1) | SEG_PRIV(0) | SEG_CODE_EXRD);
    gd_set(2, 0, 0xFFFFFFFF, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | SEG_LONG(0) | SEG_SIZE(1) | SEG_GRAN(1) | SEG_PRIV(0) | SEG_DATA_RDWR);
    gd_set(3, 0, 0xFFFFFFFF, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | SEG_LONG(0) | SEG_SIZE(1) | SEG_GRAN(1) | SEG_PRIV(3) | SEG_CODE_EXRD);
    gd_set(4, 0, 0xFFFFFFFF, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | SEG_LONG(0) | SEG_SIZE(1) | SEG_GRAN(1) | SEG_PRIV(3) | SEG_DATA_RDWR);
    gd_set(5, (uintptr_t) &sys_tss, sizeof(tss_t), 0x89);

#endif

    
    x86_lgdt();
    x86_ltr();



    int i;
    for(i = 0; i < 32; i++)
        id_set(i, isrs[i], IDT_ISR_TRAP);

    for(i = 32; i < X86_IDT_MAX; i++)
        id_set(i, isrs[i], IDT_ISR_INTERRUPT);


    for(i = 32; i < X86_IDT_MAX; i++) {
        irqs[i - 32].handler = NULL;
        spinlock_init(&irqs[i - 32].lock);
    }
    
    x86_lidt();
}



x86_frame_t* x86_isr_handler(x86_frame_t* frame) {
    static char *exception_messages[] = {
        "Division By Zero",
        "Debug",
        "Non Maskable Interrupt",
        "Breakpoint",
        "Into Detected Overflow",
        "Out of Bounds",
        "Invalid Opcode",
        "No Coprocessor",

        "Double Fault",
        "Coprocessor Segment Overrun",
        "Bad TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection Fault",
        "Page Fault",
        "Unknown Interrupt",

        "Coprocessor Fault",
        "Alignment Check",
        "Machine Check",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",

        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved"
    };

    static char* pfe_messages[] = {
        "Supervisory process tried to read a non-present page entry",
        "Supervisory process tried to read a page and caused a protection fault",
        "Supervisory process tried to write to a non-present page entry",
        "Supervisory process tried to write a page and caused a protection fault",
        "User process tried to read a non-present page entry",
        "User process tried to read a page and caused a protection fault",
        "User process tried to write to a non-present page entry",
        "User process tried to write a page and caused a protection fault",
        "Undefined behavior",
        [0x0B] = "Page fault was caused by writing in a reserved field",
        [0x0F] = "Page fault was caused by an instruction fetch",
    };



    DEBUG_ASSERT(frame);


    arch_intr_begin();

    switch(frame->int_no) {
        case 0xFE:
#ifdef ARCH_X86_64
            frame->rax = syscall_invoke(frame->rax, frame->rbx, frame->rcx, frame->rdx, frame->rsi, frame->rdi, frame->r8);
#else
            frame->eax = syscall_invoke(frame->eax, frame->ebx, frame->ecx, frame->edx, frame->esi, frame->edi, 0);
#endif
            break;
        case 0xFD:
            
            break;

        case 0xFF:
            kpanic("intr: Spourius Interrupt!");
            break;

        case 0x20:
            if(current_cpu) {
                if(unlikely(current_cpu->ticks.tv_nsec + 1000000 > 999999999)) {
                    current_cpu->ticks.tv_sec += 1;
                    current_cpu->ticks.tv_nsec = 0;
                } else
                    current_cpu->ticks.tv_nsec += 1000000;

                frame = schedule(frame);
            }

            break;

        case 0x21 ... 0xFC:

            __trylock(&irqs[frame->int_no - 0x20].lock, {

                if(likely(irqs[frame->int_no - 0x20].handler))
                    frame = (x86_frame_t*) irqs[frame->int_no - 0x20].handler((void*) frame);
                else
                    kprintf("x86-intr: Unhandled IRQ #%d caught, ignoring\n", frame->int_no - 0x20);
        
            });


            break;
        
        case 0x0E:
            kprintf ("x86-pfe: [%04p] %s at %p (%p <%s>)\n", 
                frame->err_code, 
                pfe_messages[frame->err_code], 
                x86_get_cr2(),
#if defined(ARCH_X86_64)
                frame->rip,
                core_get_name(frame->rip)
#else
                frame->eip,
                core_get_name(frame->eip)
#endif
            );

            core_stacktrace();
            x86_cli();
            for(;;);

        default:
            if(frame->int_no < 32)
                kprintf ("x86-intr: exception %d '%s' (%p)\n", 
                    frame->int_no,
                    exception_messages[frame->int_no], 
                    frame->err_code
                );
            else
                kprintf ("x86-intr: unknown exception %d (%p)\n", 
                    frame->int_no, 
                    frame->err_code
                );


            //core_stacktrace();
            x86_cli();
            for(;;);

            break;
    }



    arch_intr_end();

    return frame;
}


long arch_intr_disable(void) {
    long s;
    
    __asm__ __volatile__ (
        "pushf;"
#if defined(ARCH_X86_64)
        "pop rax;"
#else
        "pop eax;"
#endif
        "cli;"
        : "=a" (s)
        :: "memory"
    );

    return !!(s & (1 << 9));
}

void arch_intr_enable(long s) {
    if(likely(s))
        __asm__ __volatile__ ("sti");
}

int arch_intr_map_irq(uint8_t irq, void* (*handler) (void*)) {
    DEBUG_ASSERT(irq < (0xFF - 0x20));
    DEBUG_ASSERT(handler);


    __trylock(&irqs[irq].lock, {

        irqs[irq].handler = handler;
        ioapic_map_irq(irq, irq, apic_get_id());
    
    });

    return 0;
}

int arch_intr_unmap_irq(uint8_t irq) {
    DEBUG_ASSERT(irq < (0xFF - 0x20));

    __trylock(&irqs[irq].lock, {
        
        irqs[irq].handler = NULL;
        ioapic_map_irq(irq, irq, apic_get_id());

    });

    return 0;
}


void arch_intr_begin(void) {

    DEBUG_ASSERT(current_cpu ? !(current_cpu->flags & CPU_FLAGS_INTERRUPT) : 1);

    if(likely(current_cpu))
        current_cpu->flags |= CPU_FLAGS_INTERRUPT;

}


void arch_intr_end(void) {

    DEBUG_ASSERT(current_cpu ? current_cpu->flags & CPU_FLAGS_INTERRUPT : 1);

    if(likely(current_cpu))
        current_cpu->flags &= ~CPU_FLAGS_INTERRUPT;

    apic_eoi();

}