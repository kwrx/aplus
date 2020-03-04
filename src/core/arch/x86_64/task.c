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


#include <stdint.h>
#include <string.h>
#include <time.h>
#include <aplus/core/base.h>
#include <aplus/core/multiboot.h>
#include <aplus/core/debug.h>
#include <aplus/core/memory.h>
#include <aplus/core/ipc.h>
#include <aplus/core/hal.h>
#include <aplus/core/task.h>

#include <arch/x86/cpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/intr.h>
#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>



void arch_task_switch(void* __frame, task_t* prev, task_t* next) {

    DEBUG_ASSERT(prev);
    DEBUG_ASSERT(prev->frame);

    DEBUG_ASSERT(next);
    DEBUG_ASSERT(next->frame);

    DEBUG_ASSERT(__frame);


    memcpy(prev->frame, __frame, sizeof(interrupt_frame_t));
    memcpy(__frame, next->frame, sizeof(interrupt_frame_t));

}


void arch_task_spawn_init() {

    task_t* task = (task_t*) kmalloc(sizeof(task_t), GFP_KERNEL);

    task->argv = NULL;
    task->environ = NULL;

    task->tid   = 
    task->tgid  = sched_nextpid();
    task->pgid  = 1;
    task->uid   =
    task->euid  =
    task->gid   =
    task->egid  = 0;
    task->sid   = 1;

    task->status    = TASK_STATUS_READY;
    task->policy    = TASK_POLICY_RR;
    task->priority  = TASK_PRIO_REG;
    task->caps      = TASK_CAPS_SYSTEM;
    task->affinity  = ~(1 << current_cpu->id);
    
    task->frame         = (void*) kmalloc(sizeof(interrupt_frame_t), GFP_KERNEL);
    task->address_space = (void*) &current_cpu->address_space;

    task->umask = 0;

    memset(&task->clock , 0, sizeof(struct tms));
    memset(&task->sleep , 0, sizeof(struct timespec));
    memset(&task->rusage, 0, sizeof(struct rusage));
    memset(&task->fd    , 0, sizeof(struct fd) * OPEN_MAX);
    memset(&task->exit  , 0, sizeof(task->exit));
    memset(&task->iostat, 0, sizeof(task->iostat));

    spinlock_init(&task->lock);

    // TODO: Signal, VFS, RLIMIT

    
    task->next   = NULL;
    task->parent = NULL;

    if(current_cpu->id != SMP_CPU_BOOTSTRAP_ID)
        task->parent = core->bsp.running_task;
    
    current_cpu->running_task = task;


#if defined(DEBUG) && DEBUG_LEVEL >= 1
    kprintf("task: spawn init process pid(%d) cpu(%d)\n", task->tid, arch_cpu_get_current_id());
#endif

    sched_enqueue(task);

}