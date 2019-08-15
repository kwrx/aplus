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
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/smp.h>
#include <stdint.h>
#include <errno.h>


#if defined(DEBUG)

SYSCALL(404, systest,
long sys_systest (void) {

#if defined(__i386__)
    uintptr_t esp;
    uintptr_t ebp;

    __asm__ __volatile__ ("movl %%esp, %0" : "=r"(esp));
    __asm__ __volatile__ ("movl %%ebp, %0" : "=r"(ebp));

    //extern int interrupt_stack;
    kprintf("kernel-space: esp: %p, ebp: %p\n", esp, ebp);

#endif

    //kpanic("SYSCALL TEST WORK!");
    return 0;
});

#endif