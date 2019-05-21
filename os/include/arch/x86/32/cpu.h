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


#ifndef _ARCH_X86_32_CPU_H
#define _ARCH_X86_32_CPU_H

#if !defined(__i386__)
#error "__i386__ not defined, invalid arch!"
#endif

#include <stdint.h>
#include <sys/types.h>



#define x86_get_reg(n)                                                  \
    static inline uint32_t x86_get_##n() {                              \
        uint32_t ret;                                                   \
        __asm__ __volatile__ ("mov %0, " #n : "=r"(ret));               \
        return ret;                                                     \
    }
    
    x86_get_reg(eax)
    x86_get_reg(ebx)
    x86_get_reg(ecx)
    x86_get_reg(edx)
    x86_get_reg(esi)
    x86_get_reg(edi)
    x86_get_reg(ebp)
    x86_get_reg(esp)
    x86_get_reg(cr0)
    x86_get_reg(cr1)
    x86_get_reg(cr2)
    x86_get_reg(cr3)
    x86_get_reg(cr4)
    x86_get_reg(cs)
    x86_get_reg(ds)
    x86_get_reg(es)
    x86_get_reg(fs)
    x86_get_reg(gs)
    
    
static inline uint32_t x86_get_eflags() {
    uint32_t ret;
    __asm__ __volatile__(
        "pushfd; pop eax" 
        : "=a"(ret)
    );

    return ret;
}
    
    
#define x86_set_reg(n)                                                      \
    static inline void x86_set_##n(uint32_t val) {                          \
        __asm__ __volatile__ ("mov " #n ", %0" : : "r"(val));               \
    }


    x86_set_reg(eax)
    x86_set_reg(ebx)
    x86_set_reg(ecx)
    x86_set_reg(edx)
    x86_set_reg(esi)
    x86_set_reg(edi)
    x86_set_reg(ebp)
    x86_set_reg(esp)
    x86_set_reg(cr0)
    x86_set_reg(cr1)
    x86_set_reg(cr2)
    x86_set_reg(cr3)
    x86_set_reg(cr4)
    x86_set_reg(cs)
    x86_set_reg(ds)
    x86_set_reg(es)
    x86_set_reg(fs)
    x86_set_reg(gs)
    
    
static inline void x86_set_eflags(uint32_t val) {
    __asm__ __volatile__(
        "push eax; popfd" 
        : 
        : "a"(val)
    );
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


static inline void x86_wrmsr(uint32_t i, uint64_t v) {
    __asm__ __volatile__ (
        "wrmsr" 
        : 
        : "c"(i), "A"(v)
    );
}

static inline uint64_t x86_rdmsr(uint32_t i) {
    uint64_t v;
    __asm__ __volatile__ (
        "rdmsr" 
        : "=A"(v) 
        : "c"(i)
    );

    return v;
}


static inline uint64_t x86_rdtsc(void) {
    uint64_t r;
    __asm__ __volatile__ (
        "lfence; rdtsc; lfence\n" 
        : "=A"(r)
    );

    return r;
} 


static inline void x86_cpuid(long r, long* a, long* d) {
    __asm__ __volatile__ (
        "cpuid"
        : "=a"(*a), "=d"(*d) : "a"(r) 
        : "ecx", "ebx"
    );
}




extern uint32_t x86_get_eip(void);


typedef struct x86_frame {
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t int_no;
    uint32_t err_code;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t useresp;
} __attribute__((packed)) x86_frame_t;


#endif
