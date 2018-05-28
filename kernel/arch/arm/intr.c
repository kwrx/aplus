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


#include <aplus.h>
#include <aplus/debug.h>
#include <libc.h>


#ifdef __GNUC__
#define _I(s)            \
    __attribute__((interrupt (s)))
#else
#define _I(s)
#endif


void _I("UNDEF") i_undef() {
    kprintf(ERROR "PANIC! ARM Undefined istruction\n");
}

void _I("ABORT") i_abrtp() {
    kprintf(ERROR "PANIC! ARM abort prefetch\n");
}

void _I("ABORT") i_abrtd() {
    kprintf(ERROR "PANIC! ARM abort data\n");
}

void _I("SWI") i_swint(int r0, int r1, int r2, int r3) {
    register int idx;
    __asm__ (
        "ldr %0, [lr, #-4]"
        : "=r"(idx)
    );


    idx = syscall_invoke(idx & 0xFFFF, r0, r1, r2, r3);
    
    __asm__ (
        "mov r0, %0;"
        "mov r1, %1;"
        : : "r"(idx), "r"(errno)
    );
}

void _I("IRQ") i_irq() {
    return;
}

void _I("FIQ") i_fiq() {
    return;
}
