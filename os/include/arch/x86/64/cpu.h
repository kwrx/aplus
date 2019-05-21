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


#ifndef _ARCH_X86_64_CPU_H
#define _ARCH_X86_64_CPU_H

#if !defined(__x86_64__)
#error "__x86_64__ not defined, invalid arch!"
#endif

#include <stdint.h>
#include <sys/types.h>



#define x86_get_reg(n)                                                  \
    static inline uint64_t x86_get_##n() {                              \
        uint64_t ret;                                                   \
        __asm__ __volatile__ ("mov %0, " #n : "=r"(ret));               \
        return ret;                                                     \
    }
    
    x86_get_reg(rax)
    x86_get_reg(rbx)
    x86_get_reg(rcx)
    x86_get_reg(rdx)
    x86_get_reg(rsi)
    x86_get_reg(rdi)
    x86_get_reg(rbp)
    x86_get_reg(rsp)
    x86_get_reg(r8)
    x86_get_reg(r9)
    x86_get_reg(r10)
    x86_get_reg(r11)
    x86_get_reg(r12)
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
    
    
static inline uint64_t x86_get_eflags() {
    uint64_t ret;
    __asm__ __volatile__(
        "pushfd; pop rax" 
        : "=a"(ret)
    );

    return ret;
}
    
    
#define x86_set_reg(n)                                                      \
    static inline void x86_set_##n(uint64_t val) {                          \
        __asm__ __volatile__ ("mov " #n ", %0" : : "r"(val));               \
    }


    x86_set_reg(rax)
    x86_set_reg(rbx)
    x86_set_reg(rcx)
    x86_set_reg(rdx)
    x86_set_reg(rsi)
    x86_set_reg(rdi)
    x86_set_reg(rbp)
    x86_set_reg(rsp)
    x86_set_reg(r8)
    x86_set_reg(r9)
    x86_set_reg(r10)
    x86_set_reg(r11)
    x86_set_reg(r12)
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
    
    
static inline void x86_set_eflags(uint64_t val) {
    __asm__ __volatile__(
        "push rax; popfd" 
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


static inline void x86_wrmsr(uint64_t i, uint64_t v) {
    __asm__ __volatile__ (
        "wrmsr" 
        : 
        : "c"(i), "A"(v)
    );
}

static inline uint64_t x86_rdmsr(uint64_t i) {
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
        : "rcx", "rbx"
    );
}




extern uint64_t x86_get_rip(void);


typedef struct x86_frame {
    uint64_t mxcsr;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t int_no;
    uint64_t err_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t userrsp;
} __attribute__((packed)) x86_frame_t;


#endif
