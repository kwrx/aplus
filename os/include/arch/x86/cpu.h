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


#ifndef _ARCH_X86_CPU_H
#define _ARCH_X86_CPU_H

#if defined(__i386__)
#include <arch/x86/32/cpu.h>
#elif defined(__x86_64__)
#include <arch/x86/64/cpu.h>
#endif


#define IA32_PAT                0x00000277




#define x86_pause() \
    __asm__ __volatile__ ("pause")

#define x86_cli() \
    __asm__ __volatile__ ("cli")

#define x86_sti() \
    __asm__ __volatile__ ("sti")

#define x86_fxsave(r) \
    __asm__ __volatile__ ("fxsave [a]" :: "a"(r))

#define x86_fxrstor(r) \
    __asm__ __volatile__ ("fxrstor [a]" :: "a"(r))




extern uintptr_t x86_get_ip(void);

typedef struct x86_frame {
    
    char top[0];

    uintptr_t gs;
    uintptr_t fs;
    uintptr_t es;
    uintptr_t ds;
    
    uintptr_t di;
    uintptr_t si;
    uintptr_t bp;
    uintptr_t sp;
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
    uintptr_t mxcsr;
#endif

    uintptr_t int_no;
    uintptr_t err_code;
    uintptr_t ip;
    uintptr_t cs;
    uintptr_t flags;
    uintptr_t usersp;
    uintptr_t userss;

    char bottom[0];
    
} __attribute__((packed)) x86_frame_t;
#endif