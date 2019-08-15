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
extern void x86_enter_on_userspace(uintptr_t, uintptr_t, uintptr_t);


void* arch_task_init_frame(void* stack, void* ip, void* arg) {

    DEBUG_ASSERT(ip);
    DEBUG_ASSERT(stack);

    x86_frame_t* frame;
    frame = (x86_frame_t*) ((uintptr_t) stack);
    frame = (x86_frame_t*) ((uintptr_t) frame - sizeof(x86_frame_t));
    frame = (x86_frame_t*) ((uintptr_t) frame - sizeof(uintptr_t) * 5);



    frame->di = 0;
    frame->si = 0;
    frame->bp = (uintptr_t) &frame->int_no;
    frame->sp = (uintptr_t) &frame->int_no;

    frame->dx =
    frame->ax = 0;

    frame->cx = (uintptr_t) ip;
    frame->bx = (uintptr_t) arg;
    
    frame->int_no = 0;
    frame->err_code = 0;

    frame->ip = (uintptr_t) &x86_enter_on_clone;
    frame->cs = 0x08;
    frame->flags = 0x202;
    frame->usersp = 0;


    return &frame->top;
}

    
void* arch_task_switch(void* context, task_t* from, task_t* to) {

    DEBUG_ASSERT(to);
    DEBUG_ASSERT(from);
    DEBUG_ASSERT(context);


    from->frame.context = context;

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



    arch_intr_set_stack((uintptr_t) &to->frame.top);

    return to->frame.context;
}


void arch_task_enter_on_userspace(uintptr_t ip, uintptr_t stack, uintptr_t arg) {
    x86_enter_on_userspace(ip, stack, arg);
}


void task_init(void) {    

}