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
#include <aplus/memory.h>
#include <aplus/ipc.h>
#include <aplus/task.h>

#include <hal/cpu.h>
#include <hal/interrupt.h>
#include <hal/vmm.h>
#include <hal/task.h>

#include <arch/x86/cpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/intr.h>
#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>

#include <stdint.h>
#include <string.h>
#include <time.h>



#define FRAME(p)    \
    ((interrupt_frame_t*) p->frame)



void arch_task_switch(void* __frame, task_t* prev, task_t* next) {

    DEBUG_ASSERT(prev);
    DEBUG_ASSERT(prev->frame);

    DEBUG_ASSERT(next);
    DEBUG_ASSERT(next->frame);

    DEBUG_ASSERT(__frame);


    memcpy(prev->frame, __frame, sizeof(interrupt_frame_t));
    memcpy(__frame, next->frame, sizeof(interrupt_frame_t));

    // TODO: Switch FPU Registers

}


pid_t arch_task_spawn_init() {

    task_t* task = (task_t*) kmalloc(sizeof(task_t), GFP_KERNEL);


    static char* __argv[2] = { "init", NULL };
    static char* __envp[1] = { NULL };

    task->argv = __argv;
    task->environ = __envp;


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


    extern inode_t __vfs_root;

    task->cwd  =
    task->root = &__vfs_root;
    task->exe  = NULL;

    task->umask = 0;



    memset(&task->clock , 0, sizeof(struct tms));
    memset(&task->sleep , 0, sizeof(struct timespec));
    memset(&task->rusage, 0, sizeof(struct rusage));
    memset(&task->fd    , 0, sizeof(struct fd) * OPEN_MAX);
    memset(&task->exit  , 0, sizeof(task->exit));
    memset(&task->iostat, 0, sizeof(task->iostat));

    spinlock_init(&task->lock);

    // TODO: Signal, RLIMIT


    task->next   = NULL;
    task->parent = NULL;

    if(current_cpu->id != SMP_CPU_BOOTSTRAP_ID)
        task->parent = core->bsp.running_task;
    
    current_cpu->running_task = task;


#if defined(DEBUG) && DEBUG_LEVEL >= 1
    kprintf("task: spawn init process pid(%d) cpu(%d)\n", task->tid, arch_cpu_get_current_id());
#endif

    sched_enqueue(task);

    return task->tid;
}


pid_t arch_task_spawn_kthread(const char* name, void (*entry) (void*), size_t stacksize, void* arg) {

    DEBUG_ASSERT(name);
    DEBUG_ASSERT(entry);
    DEBUG_ASSERT(stacksize);
    


    task_t* task = (task_t*) kmalloc(sizeof(task_t), GFP_KERNEL);


    task->argv = (char**) kmalloc((sizeof(char*) * 2) + strlen(name) + 1, GFP_KERNEL);

    task->argv[0] = (char*) &task->argv[2];
    task->argv[1] = (char*) NULL;

    strcpy(task->argv[0], name);


    task->environ = current_task->environ;

    task->tid   = sched_nextpid();
    task->tgid  = current_task->tid;
    task->pgid  = current_task->pgid;
    task->uid   = current_task->uid;
    task->euid  = current_task->euid;
    task->gid   = current_task->gid;
    task->egid  = current_task->egid;
    task->sid   = current_task->sid;

    task->status    = TASK_STATUS_READY;
    task->policy    = current_task->policy;
    task->priority  = current_task->priority;
    task->caps      = current_task->caps;
    task->affinity  = 0;
    
    task->frame         = (void*) kmalloc(sizeof(interrupt_frame_t), GFP_KERNEL);
    task->address_space = (void*) &current_cpu->address_space;

    task->cwd  = current_task->cwd;
    task->root = current_task->root;
    task->exe  = current_task->exe;

    task->umask = 0;


    memset(&task->clock  , 0, sizeof(struct tms));
    memset(&task->sleep  , 0, sizeof(struct timespec));
    memset(&task->rusage , 0, sizeof(struct rusage));
    memset(&task->exit   , 0, sizeof(task->exit));
    memset(&task->iostat , 0, sizeof(task->iostat));

    memcpy(&task->fd, &current_task->fd, sizeof(struct fd) * OPEN_MAX);
    memcpy(&task->rlimits, &current_task->rlimits, sizeof(struct rlimit) * RLIM_NLIMITS);


    // TODO: Signal, RLIMIT


    FRAME(task)->ip = (uintptr_t) entry;
    FRAME(task)->cs = KERNEL_CS;
    FRAME(task)->flags = 0x200;
    FRAME(task)->user_sp = (uintptr_t) kmalloc(stacksize, GFP_KERNEL) + stacksize;
    FRAME(task)->user_ss = KERNEL_DS;

#if defined(__x86_64__)
    FRAME(task)->di = (uintptr_t) arg;
#else
    (*(uintptr_t*) FRAME(task)->user_sp)-- = (uintptr_t) NULL;
    (*(uintptr_t*) FRAME(task)->user_sp)-- = (uintptr_t) arg;
#endif


    spinlock_init(&task->lock);
    
    task->next   = NULL;
    task->parent = current_task;
    

#if defined(DEBUG) && DEBUG_LEVEL >= 1
    kprintf("task: spawn kthread %s pid(%d) stacksize(%p)\n", task->argv[0], task->tid, stacksize);
#endif

    sched_enqueue(task);

    return task->tid;
} 