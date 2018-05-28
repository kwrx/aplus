/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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


#ifndef _I386_H
#define _I386_H

#if !defined(__i386__)
#error "__i386__ not defined, invalid arch!"
#endif

#include <aplus.h>
#include <aplus/mm.h>
#include <libc.h>


#define TIMER_FREQ            1000


#define read_reg(n)                                                 \
    static inline uint32_t read_##n() {                             \
        uint32_t ret;                                               \
        __asm__ __volatile__ ("mov %0, " #n : "=r"(ret));           \
        return ret;                                                 \
    }
    
    read_reg(eax)
    read_reg(ebx)
    read_reg(ecx)
    read_reg(edx)
    read_reg(esi)
    read_reg(edi)
    read_reg(ebp)
    read_reg(esp)
    read_reg(cr0)
    read_reg(cr1)
    read_reg(cr2)
    read_reg(cr3)
    read_reg(cr4)
    read_reg(cs)
    read_reg(ds)
    read_reg(es)
    read_reg(fs)
    read_reg(gs)
    
    
static inline uint32_t read_eflags() {
    uint32_t ret;
    __asm__ __volatile__("pushfd; pop eax" : "=a"(ret));
    return ret;
}
    
    
#define write_reg(n)                                                    \
    static inline void write_##n(uint32_t val) {                        \
        __asm__ __volatile__ ("mov " #n ", %0" : : "r"(val));           \
    }


    write_reg(eax)
    write_reg(ebx)
    write_reg(ecx)
    write_reg(edx)
    write_reg(esi)
    write_reg(edi)
    write_reg(ebp)
    write_reg(esp)
    write_reg(cr0)
    write_reg(cr1)
    write_reg(cr2)
    write_reg(cr3)
    write_reg(cr4)
    write_reg(cs)
    write_reg(ds)
    write_reg(es)
    write_reg(fs)
    write_reg(gs)
    
    
static inline void write_eflags(uint32_t val) {
    __asm__ __volatile__("push eax; popfd" : : "a"(val));
}    

#define outx(n, t, reg)                                                 \
    static inline void out##n(uint16_t p, t v) {                        \
        __asm__ __volatile__ ("out dx, " #reg : : "a"(v), "d"(p));      \
    }
    
    outx(b, uint8_t, al)
    outx(w, uint16_t, ax)
    outx(l, uint32_t, eax)
    
    
#define inx(n, t, reg)                                                      \
    static inline t in##n(uint16_t p) {                                     \
        t r;                                                                \
        __asm__ __volatile__ ("in " #reg ", dx" : "=a"(r) : "d"(p));        \
        return r;                                                           \
    }
    
    inx(b, uint8_t, al)
    inx(w, uint16_t, ax)
    inx(l, uint32_t, eax)
    
#define outsx(n, t, reg)                                                        \
    static inline void outs##n(uint16_t p, t* v, uint32_t len) {                \
        for(int i = 0; i < len; i++)                                            \
            out##n(p, v[i]);                                                    \
    }
    
    outsx(b, uint8_t, al)
    outsx(w, uint16_t, ax)
    outsx(l, uint32_t, eax)
    
    
#define insx(n, t, reg)                                                         \
    static inline t* ins##n(uint16_t p, t* v, uint32_t len) {                   \
        for(int i = 0; i < len; i++)                                            \
            v[i] = in##n(p);                                                    \
                                                                                \
        return v;                                                               \
    }
    
    insx(b, uint8_t, al)
    insx(w, uint16_t, ax)
    insx(l, uint32_t, eax)


static inline void wrmsr(uint32_t i, uint64_t v) {
    __asm__ __volatile__ ("wrmsr" : : "c"(i), "A"(v));
}

static inline uint64_t rdmsr(uint32_t i) {
    uint64_t v;
    __asm__ __volatile__ ("rdmsr" : "=A"(v) : "c"(i));
    return v;
}


static inline uint64_t rdtsc(void) {
    uint64_t r;
    __asm__ __volatile__ (
        "lfence; rdtsc; lfence\n" : "=A"(r));
    return r;
} 


static inline void cpuid(long r, long* a, long* d) {
    __asm__ __volatile__ ("cpuid" : "=a"(*a), "=d"(*d) : "a"(r) : "ecx", "ebx");
}
    
typedef struct i386_context {
    uint32_t gs, fs, es, ds, edi, esi, ebp, esp, ebx, edx, ecx, eax, int_no, err_code, eip, cs, eflags, useresp;
} __attribute__((packed)) i386_context_t;

extern uintptr_t read_eip();
extern void x86_intr_kernel_stack(uintptr_t address);





typedef void (irq_handler_t) (void*);


void irq_enable(int number, irq_handler_t handler);
void irq_disable(int number);

void* irq_set_data(int number, void* data);
void* irq_get_data(int number);

void irq_ack(int irq_no);

extern int current_irq;

#endif
