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


#define IA32_PAT                0x00000277
#endif