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
#include <aplus/intr.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/syscall.h>

#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>
#include <arch/x86/apic.h>
#include <stdint.h>



static struct {
    
    struct {

        uint16_t base_low;
        uint16_t selector;
        uint8_t zero;
        uint8_t flags;
        uint16_t base_high;

    #if defined(__x86_64__)
        uint32_t base_xhigh;
        uint32_t padding;
    #endif

    } __packed e[X86_IDT_MAX];


    struct {
        
        uint16_t size;
        uintptr_t addr;

    } __packed ptr;

} __packed idtp;


static struct {

    void* (*handler) (void*);
    spinlock_t lock;

} __packed irqs[X86_IDT_MAX - 32];


static struct {
    
    struct {
        
        union {
            uint8_t b[8];
            uint64_t d;
        } __packed;

    } __packed e[X86_GDT_MAX];

    struct {
        
        uint16_t size;
        uintptr_t addr;

    } __packed ptr;

} __packed gdtp[CPU_MAX];


static tss_t tss[CPU_MAX];

extern uintptr_t isrs[X86_IDT_MAX];
extern uintptr_t core_stack_area;



void x86_gdt_init_percpu(uint16_t cpu) {

    DEBUG_ASSERT(cpu < CPU_MAX);


    inline void __set(int index, uintptr_t base, uintptr_t limit, uint16_t type) {
                
        if(limit > 0xFFFF)
            limit >>= 12;

        gdtp[cpu].e[index].b[0] = (limit >> 0) & 0xFF;
        gdtp[cpu].e[index].b[1] = (limit >> 8) & 0xFF;
        gdtp[cpu].e[index].b[6] = (limit >> 16) & 0x0F;
        
        gdtp[cpu].e[index].b[2] = (base >> 0) & 0xFF;
        gdtp[cpu].e[index].b[3] = (base >> 8) & 0xFF;
        gdtp[cpu].e[index].b[4] = (base >> 16) & 0xFF;
        gdtp[cpu].e[index].b[7] = (base >> 24) & 0xFF;

        gdtp[cpu].e[index].b[6] |= (type >> 8) & 0xF0;
        gdtp[cpu].e[index].b[5] =  (type >> 0) & 0xFF;

    }




#if defined(__x86_64__)

    __set(0, 0, 0, 0);
    __set(1, 0, 0, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_PRIV(0) | SEG_LONG(1) | SEG_CODE_EXRD);
    __set(2, 0, 0, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_PRIV(0) | SEG_LONG(1) | SEG_DATA_RDWR);
    __set(3, 0, 0, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_PRIV(3) | SEG_LONG(1) | SEG_CODE_EXRD);
    __set(4, 0, 0, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_PRIV(3) | SEG_LONG(1) | SEG_DATA_RDWR);
    __set(5, (uintptr_t) &tss[cpu] & 0xFFFFFFFF, sizeof(tss_t), 0xE9);
    
    gdtp[cpu].e[6].d = (((uintptr_t) &tss[cpu]) >> 32) & 0xFFFFFFFF;


#elif defined(__i386__)

    tss[cpu].esp0 = 0x00;
    tss[cpu].ss0  = 0x10;
    tss[cpu].cs   = 0x0B;
    tss[cpu].ds   = 0x13;
    tss[cpu].ss   = 0x13;
    tss[cpu].es   = 0x13;
    tss[cpu].fs   = 0x13;
    tss[cpu].gs   = 0x13;
    tss[cpu].iomap_base = sizeof(tss_t);


    __set(0, 0, 0, 0);
    __set(1, 0, 0xFFFFFFFF, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | SEG_LONG(0) | SEG_SIZE(1) | SEG_GRAN(1) | SEG_PRIV(0) | SEG_CODE_EXRD);
    __set(2, 0, 0xFFFFFFFF, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | SEG_LONG(0) | SEG_SIZE(1) | SEG_GRAN(1) | SEG_PRIV(0) | SEG_DATA_RDWR);
    __set(3, 0, 0xFFFFFFFF, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | SEG_LONG(0) | SEG_SIZE(1) | SEG_GRAN(1) | SEG_PRIV(3) | SEG_CODE_EXRD);
    __set(4, 0, 0xFFFFFFFF, SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | SEG_LONG(0) | SEG_SIZE(1) | SEG_GRAN(1) | SEG_PRIV(3) | SEG_DATA_RDWR);
    __set(5, (uintptr_t) &tss[cpu], sizeof(tss_t), 0xE9);

#endif


    gdtp[cpu].ptr.size = sizeof(gdtp[cpu].e) - 1;
    gdtp[cpu].ptr.addr = (uintptr_t) &gdtp[cpu].e[0];


    __asm__ __volatile__("lgdt [%0]" :: "r"(&gdtp[cpu].ptr));
    __asm__ __volatile__("ltr ax"    :: "a"(0x2B));



#if defined(__i386__)
    __asm__ __volatile__ (
        "jmp 0x08:L1    \n"
        "L1:            \n"
        "mov cx, 0x10   \n"
        "mov ds, cx     \n"
        "mov es, cx     \n"
        "mov fs, cx     \n"
        "mov gs, cx     \n"
        "mov ss, cx     \n"
    
        ::: "ecx"
    );
#endif

}


void x86_idt_init_percpu(uint16_t cpu) {


    inline void __set(int index, uintptr_t isr, uint8_t type) {
#if defined(__x86_64__)
        idtp.e[index].base_low = (isr) & 0xFFFF;
        idtp.e[index].base_high = (isr >> 16) & 0xFFFF;
        idtp.e[index].base_xhigh = (isr >> 32) & 0xFFFFFFFF;
        idtp.e[index].selector = 0x08;
        idtp.e[index].padding = 0;
        idtp.e[index].flags = type;
#else
        idtp.e[index].base_low = (isr) & 0xFFFF;
        idtp.e[index].base_high = (isr >> 16) & 0xFFFF;
        idtp.e[index].selector = 0x08;
        idtp.e[index].zero = 0;
        idtp.e[index].flags = type;
#endif
    }


    if(cpu == 0) {

        int i;
        for(i = 0; i < 32; i++)
            __set(i, isrs[i], IDT_ISR_TRAP        | IDT_ISR_DPL(0));

        for(i = 32; i < X86_IDT_MAX - 32; i++)
            __set(i, isrs[i], IDT_ISR_INTERRUPT   | IDT_ISR_DPL(0));

        
        /* Syscalls */
        __set(0xFD, isrs[0xFD], IDT_ISR_INTERRUPT | IDT_ISR_DPL(3));
        __set(0xFE, isrs[0xFE], IDT_ISR_INTERRUPT | IDT_ISR_DPL(3));

        /* NMI */
        __set(0xFF, isrs[0xFF], IDT_ISR_INTERRUPT | IDT_ISR_DPL(0));



        for(i = 32; i < X86_IDT_MAX; i++) {

            irqs[i - 32].handler = NULL;
            spinlock_init(&irqs[i - 32].lock);
        
        }


        idtp.ptr.size = sizeof(idtp.e) - 1;
        idtp.ptr.addr = (uintptr_t) &idtp;

    }


    __asm__ __volatile__("lidt [%0]" :: "r"(&(idtp.ptr)));

}


void intr_init(void) {

    memset(&gdtp, 0, sizeof(gdtp));
    memset(&idtp, 0, sizeof(idtp));
    memset(&irqs, 0, sizeof(irqs));
    memset(&tss,  0, sizeof(tss));

    x86_gdt_init_percpu(0);
    x86_idt_init_percpu(0);

}



x86_frame_t* x86_isr_handler(x86_frame_t* frame) {

    DEBUG_ASSERT(frame);


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




    switch(frame->int_no) {

        case 0x20 ... 0xFF:
            break;


        case 0x0E:
            
            kpanic ("x86-pfe: [%04p] %s at %p (%p <%s>)", 
                frame->err_code, 
                pfe_messages[frame->err_code], 
                x86_get_cr2(),
                frame->ip,
                core_get_name(frame->ip)
            );

        default:

            if(frame->int_no < 32)
                kpanic ("x86-intr: exception %d '%s' (%p)", 
                    frame->int_no,
                    exception_messages[frame->int_no], 
                    frame->err_code
                );
            else
                kpanic ("x86-intr: unknown exception %d (%p)", 
                    frame->int_no, 
                    frame->err_code
                );

    }



    if(unlikely(!current_cpu))
        return frame;



    DEBUG_ASSERT(!(current_cpu->flags & CPU_FLAGS_INTERRUPT));

    current_cpu->flags |= CPU_FLAGS_INTERRUPT;
    current_task->frame.context = frame;

    
    switch(frame->int_no) {
        
        case 0xFE:

#if defined(__x86_64__)
            frame->ax = syscall_invoke(frame->ax, frame->bx, frame->cx, frame->dx, frame->si, frame->di, frame->r8);
#elif defined(__i386__)
            frame->ax = syscall_invoke(frame->ax, frame->bx, frame->cx, frame->dx, frame->si, frame->di, 0);
#endif

            break;

        case 0xFD:
            
            break;

        case 0xFF:
            kpanic("intr: Spourius Interrupt on cpu #%d!", current_cpu->id);
            break;

        case 0x20:
            
            if(unlikely(current_cpu->ticks.tv_nsec + 1000000 > 999999999)) {
                current_cpu->ticks.tv_sec += 1;
                current_cpu->ticks.tv_nsec = 0;
            } else
                current_cpu->ticks.tv_nsec += 1000000;

            
            if(likely(current_cpu->flags & CPU_FLAGS_SCHED_ENABLED))
                frame = schedule();
            
            apic_eoi();

            break;

        case 0x21 ... 0xFC:

            __trylock(&irqs[frame->int_no - 0x20].lock, {

                if(likely(irqs[frame->int_no - 0x20].handler))
                    frame = (x86_frame_t*) irqs[frame->int_no - 0x20].handler((void*) frame);
                else
                    kprintf("x86-intr: Unhandled IRQ #%d caught, ignoring\n", frame->int_no - 0x20);
                        
            });

            apic_eoi();
            break;
        

        default:

            kpanic("x86-intr: ERROR! invalid interrupt #%d\n", frame->int_no);
            break;

    }



    DEBUG_ASSERT(current_cpu->flags & CPU_FLAGS_INTERRUPT);

    current_cpu->flags &= ~CPU_FLAGS_INTERRUPT;


    return frame;
}


long arch_intr_disable(void) {
    long s;
    
    __asm__ __volatile__ (
        "pushf;"
#if defined(__x86_64__)
        "pop rax;"
#elif defined(__i386__)
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


void arch_intr_set_stack(uintptr_t stack) {
    tss[apic_get_id()].esp0 = stack;
}