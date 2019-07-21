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


#if defined(__i386__)

    frame->gs =
    frame->fs =
    frame->es =
    frame->ds = 0x10;

    frame->edi = 0;
    frame->esi = 0;
    frame->ebp = (uintptr_t) &frame->int_no;
    frame->esp = (uintptr_t) &frame->int_no;

    frame->edx =
    frame->eax = 0;

    frame->ecx = (uintptr_t) ip;
    frame->ebx = (uintptr_t) arg;
    
    frame->int_no = 0;
    frame->err_code = 0;

    frame->eip = (uintptr_t) &x86_enter_on_clone;
    frame->cs = 0x08;
    frame->eflags = 0x202;
    frame->useresp = 0;


#elif defined(__x86_64__)

#endif

    return &frame->top;
}


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


void arch_task_enter_on_userspace(uintptr_t ip, uintptr_t stack, uintptr_t arg) {
    x86_enter_on_userspace(ip, stack, arg);
}

void task_init(void) {    

}