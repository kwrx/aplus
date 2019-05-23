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
#include <aplus/mm.h>
#include <aplus/timer.h>
#include <aplus/task.h>
#include <aplus/smp.h>
#include <arch/x86/apic.h>
#include <arch/x86/cpu.h>
#include <arch/x86/smp.h>
#include <arch/x86/mm.h>
#include <stdint.h>
#include <string.h>

static heap_t* heap_task;

void* arch_task_switch(void* frame, task_t* from, task_t* to) {

    from->context.frame = frame;

    __asm__ __volatile__ (
        "fsave [%0];"
        "frstor [%1];"

        :: "r"(from->context.fpu), "r"(to->context.fpu)
    );


    DEBUG_ASSERT(from->aspace);
    DEBUG_ASSERT(to->aspace);
    DEBUG_ASSERT(to->context.frame);


    if(unlikely(from->aspace->vmmpd != to->aspace->vmmpd))
        x86_set_cr3(to->aspace->vmmpd);


    return to->context.frame;
}

void task_init(void) {    
    heap_create(&heap_task, sizeof(task_t), TASK_NPROC);

    if(unlikely(!heap_task))
        kpanic("heap_create(): could not create heap for task, size: %d\n", TASK_NPROC * sizeof(task_t));
}