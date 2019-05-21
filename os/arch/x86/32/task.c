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
#include <aplus/task.h>
#include <stdint.h>

/* ops.asm */
extern void x86_switch(uintptr_t*, uintptr_t*, uintptr_t, uintptr_t, int);

void arch_task_initialize_context(task_t* task) {
    DEBUG_ASSERT(task);
    DEBUG_ASSERT(task->context.ip);

    uintptr_t* stack = (void*) &task->context.stack[128];
    *--stack = 0x0202;
    *--stack = 0x0008;
    *--stack = task->context.ip;
    
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = (uintptr_t) &task->context.stack[128];

    *--stack = 0x10;
    *--stack = 0x10;
    *--stack = 0x10;
    *--stack = 0x10;

    task->context.sp = (uintptr_t) stack;
}


void arch_task_switch(task_t* from, task_t* to) {
    DEBUG_ASSERT(from);
    DEBUG_ASSERT(to);

    x86_switch(&from->context.sp, &from->aspace->vmmpd, to->context.sp, to->aspace->vmmpd, 1);
}

