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

/* See arch/x86/BITS/ops.asm */
extern void x86_enter_on_clone(void);



void arch_task_set_context(void* context, void* ip, void* arg, void* stack, int caps) {

    DEBUG_ASSERT(context);
    DEBUG_ASSERT(ip);
    DEBUG_ASSERT(stack);


    x86_frame_t* frame;
    frame = (x86_frame_t*) ((uintptr_t) context);

    frame->di = 0;
    frame->si = 0;
    frame->bp = (uintptr_t) stack;
    
    frame->ax = (uintptr_t) arg;
    frame->bx = 0;
    frame->cx = 0;
    frame->dx = 0;

    frame->int_no = 0;
    frame->err_code = 0;

    frame->ip = (uintptr_t) ip;
    frame->sp = (uintptr_t) stack;
    frame->flags = 0x202;

    
    if(caps & TASK_CAPS_SYSTEM) {
        
        frame->gs = 0x10;
        frame->fs = 0x10;
        frame->es = 0x10;
        frame->ds = 0x10;
        frame->ss = 0x10;
        frame->cs = 0x08;

    } else {

        frame->gs = 0x23;
        frame->fs = 0x23;
        frame->es = 0x23;
        frame->ds = 0x23;
        frame->ss = 0x23;
        frame->cs = 0x1B;

    }

}


void arch_task_return_to_context(void* context) {

#if defined(__i386__)

    __asm__ __volatile__ (
        "mov esp, eax       \n"
        "pop gs             \n"
        "pop fs             \n"
        "pop es             \n"
        "pop ds             \n"
        "popad              \n"
        "add esp, 8         \n"
        "iretd              \n"
    
        :: "a"(context)
    );

#elif defined(__x86_64__)

#endif

}


void* arch_task_switch(task_t* from, task_t* to) {

    DEBUG_ASSERT(to);
    DEBUG_ASSERT(from);


    __asm__ __volatile__ (
        "fsave [%0];"
        "frstor [%1];"

        :: "r"(&from->frame.fpu), "r"(&to->frame.fpu)
    );


    DEBUG_ASSERT(from->aspace);
    DEBUG_ASSERT(from->frame.context);

    DEBUG_ASSERT(to->aspace);
    DEBUG_ASSERT(to->frame.context);


    if(unlikely(from->aspace->vmmpd != to->aspace->vmmpd))
        x86_set_cr3(to->aspace->vmmpd);



    arch_intr_set_stack((uintptr_t) &to->frame.bottom);

    return to->frame.context;
}


void task_init(void) {    

}