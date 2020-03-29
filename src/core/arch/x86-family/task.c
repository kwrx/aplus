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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/ipc.h>
#include <aplus/task.h>
#include <aplus/hal.h>

#include <arch/x86/cpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/intr.h>
#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>



#define FRAME(p)            \
    ((interrupt_frame_t*) p->frame)

#define WRITE_SP0(cpu, ptr)                         \
    cpu->sp0 = (void*) ((uintptr_t) (ptr) & ~15);   \
    ((tss_t*) cpu->tss)->sp0 = (uintptr_t) cpu->sp0



void arch_task_switch(task_t* prev, task_t* next) {

    DEBUG_ASSERT(current_cpu->frame);

    DEBUG_ASSERT(prev);
    DEBUG_ASSERT(prev->frame);
    DEBUG_ASSERT(prev->address_space->pm);

    DEBUG_ASSERT(next);
    DEBUG_ASSERT(next->frame);
    DEBUG_ASSERT(next->address_space->pm);



    if(unlikely(next->flags & TASK_FLAGS_NO_FRAME)) {

        memcpy(next->frame, current_cpu->frame, sizeof(interrupt_frame_t));
        next->flags &= ~TASK_FLAGS_NO_FRAME;

    }



    if(likely(prev != next)) {

        //kprintf("%s(%d) %p -> %s(%d) %p\n", prev->argv[0], prev->tid, FRAME(prev)->ip, next->argv[0], next->tid, FRAME(next)->ip);
        
        memcpy(prev->frame, current_cpu->frame, sizeof(interrupt_frame_t));
        memcpy(current_cpu->frame, next->frame, sizeof(interrupt_frame_t));

        if(unlikely(prev->address_space->pm != next->address_space->pm))
            x86_set_cr3(next->address_space->pm);

    }



    WRITE_SP0(current_cpu, next->sp0);


    x86_wrmsr(X86_MSR_FSBASE, next->userspace.thread_area);
    //x86_wrmsr(X86_MSR_GSBASE, next->userspace.cpu_area);

    __asm__ __volatile__ ("fxsave (%0)" :: "r"(prev->fpu));
    __asm__ __volatile__ ("fxrstor (%0)" :: "r"(next->fpu));

    
}



pid_t arch_task_spawn_init() {

    task_t* task = (task_t*) kcalloc (
        1, (sizeof(task_t))                                 // TCB
         + (sizeof(interrupt_frame_t))                      // Registers
         + (KERNEL_SYSCALL_STACKSIZE)                       // Kernel Stack
    , GFP_KERNEL);



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
    task->flags     = TASK_FLAGS_NO_FRAME;
    task->affinity  = ~(1 << current_cpu->id);

    
    task->frame         = (void*) ((uintptr_t) task + sizeof(task_t));
    task->sp0           = (void*) ((uintptr_t) task + sizeof(task_t) + sizeof(interrupt_frame_t) + KERNEL_SYSCALL_STACKSIZE);
    task->fpu           = (void*) arch_vmm_p2v(pmm_alloc_block(), ARCH_VMM_AREA_HEAP);
    task->address_space = (void*) &current_cpu->address_space;

    current_cpu->address_space.refcount++;


    extern inode_t __vfs_root;

    task->cwd  =
    task->root = &__vfs_root;
    task->exe  = NULL;

    task->umask = 0;



    memset(&task->clock , 0, sizeof(struct tms));
    memset(&task->sleep , 0, sizeof(struct timespec));
    memset(&task->rusage, 0, sizeof(struct rusage));
    memset(&task->fd    , 0, sizeof(struct fd) * CONFIG_OPEN_MAX);
    memset(&task->exit  , 0, sizeof(task->exit));
    memset(&task->iostat, 0, sizeof(task->iostat));

    spinlock_init(&task->lock);
    spinlock_init(&task->sched_lock);



    int j;
    for(j = 0; j < RLIM_NLIMITS; j++)
        task->rlimits[j].rlim_cur =
        task->rlimits[j].rlim_max = RLIM_INFINITY;

    task->rlimits[RLIMIT_STACK].rlim_cur = KERNEL_STACK_SIZE;



    // TODO: Signal


    task->next   = NULL;
    task->parent = NULL;


    if(current_cpu->id != SMP_CPU_BOOTSTRAP_ID)
        task->parent = core->bsp.running_task;
    
    current_cpu->running_task = task;

    WRITE_SP0(current_cpu, task->sp0);



#if defined(DEBUG) && DEBUG_LEVEL >= 1
    kprintf("task: spawn init process pid(%d) cpu(%d) sp0(%p)\n", task->tid, arch_cpu_get_current_id(), task->sp0);
#endif


    spinlock_lock(&task->sched_lock);

    sched_enqueue(task);

    return task->tid;
}


pid_t arch_task_spawn_kthread(const char* name, void (*entry) (void*), size_t stacksize, void* arg) {

    DEBUG_ASSERT(name);
    DEBUG_ASSERT(entry);
    DEBUG_ASSERT(stacksize);
    


    task_t* task = (task_t*) kcalloc (
        1, (sizeof(task_t))                                 // TCB
         + (sizeof(char*) * 2) + strlen(name) + 1           // Arguments & Environment
         + (sizeof(interrupt_frame_t))                      // Registers
         + (stacksize)                                      // Stack
         + (KERNEL_SYSCALL_STACKSIZE)                       // Kernel Stack
    , GFP_KERNEL);


    uintptr_t offset_of_argv  = ((uintptr_t) task + sizeof(task_t));
    uintptr_t offset_of_frame = ((uintptr_t) task + sizeof(task_t) + (sizeof(char*) * 2) + strlen(name) + 1);
    uintptr_t offset_of_stack = ((uintptr_t) task + sizeof(task_t) + (sizeof(char*) * 2) + strlen(name) + 1 + sizeof(interrupt_frame_t) + stacksize);
    uintptr_t offset_of_sp0   = ((uintptr_t) task + sizeof(task_t) + (sizeof(char*) * 2) + strlen(name) + 1 + sizeof(interrupt_frame_t) + stacksize + KERNEL_SYSCALL_STACKSIZE);



    task->argv = (char**) offset_of_argv;

    task->argv[0] = (char*) &task->argv[2];
    task->argv[1] = (char*) NULL;

    strcpy(task->argv[0], name);


    task->environ = current_task->environ;

    task->tid   = sched_nextpid();
    task->tgid  = current_task->tgid;
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
    task->flags     = 0;
    task->affinity  = 0;
    

    task->frame         = (void*) offset_of_frame;
    task->sp0           = (void*) offset_of_sp0;
    task->fpu           = (void*) arch_vmm_p2v(pmm_alloc_block(), ARCH_VMM_AREA_HEAP);
    task->address_space = (void*) current_task->address_space;

    current_task->address_space->refcount++;


    task->cwd  = current_task->cwd;
    task->root = current_task->root;
    task->exe  = current_task->exe;

    task->umask = current_task->umask;


    memset(&task->clock  , 0, sizeof(struct tms));
    memset(&task->sleep  , 0, sizeof(struct timespec));
    memset(&task->rusage , 0, sizeof(struct rusage));
    memset(&task->exit   , 0, sizeof(task->exit));
    memset(&task->iostat , 0, sizeof(task->iostat));

    memcpy(&task->fd, &current_task->fd, sizeof(struct fd) * CONFIG_OPEN_MAX);
    memcpy(&task->rlimits, &current_task->rlimits, sizeof(struct rlimit) * RLIM_NLIMITS);


    // TODO: Signal


    FRAME(task)->ip = (uintptr_t) entry;
    FRAME(task)->cs = KERNEL_CS;
    FRAME(task)->flags = 0x202;
    FRAME(task)->sp = offset_of_stack;
    FRAME(task)->ss = KERNEL_DS;

#if defined(__x86_64__)
    FRAME(task)->di = (uintptr_t) arg;
#else
    (*(uintptr_t*) FRAME(task)->sp)-- = (uintptr_t) NULL;
    (*(uintptr_t*) FRAME(task)->sp)-- = (uintptr_t) arg;
#endif


    spinlock_init(&task->lock);
    spinlock_init(&task->sched_lock);
    

    task->next   = NULL;
    task->parent = current_task;
    

#if defined(DEBUG) && DEBUG_LEVEL >= 1
    kprintf("task: spawn kthread %s pid(%d) ip(%p) sp0(%p) stacksize(%p)\n", task->argv[0], task->tid, entry, task->sp0, stacksize);
#endif

    sched_enqueue(task);

    return task->tid;
}

