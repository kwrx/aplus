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
#include <aplus/base.h>
#include <aplus/mm.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <aplus/task.h>
#include <aplus/intr.h>
#include <aplus/utils/list.h>
#include <libc.h>

#include <arch/i386/i386.h>
#include "mm/paging.h"


#define FPU(x)        ((void*) (((uintptr_t) (x) & 0x200) + 0x200))
#define CTX(x)        ((i386_task_context_t*) x->context)


typedef struct i386_task_context {
    uintptr_t esp;
    uintptr_t ebp;
    uintptr_t eip;
    uintptr_t vmmpd;
    uint8_t fpu[1024];
    uint8_t sigstack[128];
} i386_task_context_t;

extern int end;



extern void return_from_fork(void);
extern void return_from_clone(void);
extern void return_from_clone_and_exit(void);
__asm__ (
    ".globl return_from_fork        \n"
    "return_from_fork:              \n"
    "    pop gs                     \n"
    "    pop fs                     \n"
    "    pop es                     \n"
    "    pop ds                     \n"
    "    popa                       \n"
    "    add esp, 8                 \n"
    "    mov dx, 0x20               \n"
    "    mov al, 0x20               \n"
    "    out dx, al                 \n"
    "    xor eax, eax               \n"
    "    sti                        \n"
    "iret                           \n"
);


__asm__ (
    ".globl return_from_clone_and_exit          \n"
    "return_from_clone_and_exit:                \n"
    "    push eax                               \n"
    "    call sys_exit                          \n"
    ".L0:                                       \n"
    "    jmp .L0                                \n"
);

__asm__ (
    ".globl return_from_clone       \n"
    "return_from_clone:             \n"
    "    mov dx, 0x20               \n"
    "    mov al, 0x20               \n"
    "    out dx, al                 \n"
    "    sti                        \n"
    "    pop eax                    \n"
    "    jmp eax                    \n"
);

extern char** args_dup(char**);

void fork_handler(i386_context_t* context) {
    INTR_OFF;

    volatile task_t* child = (volatile task_t*) kmalloc(sizeof(task_t), GFP_KERNEL);
    KASSERT(child);

    memset((void*) child, 0, sizeof(task_t));

    child->pid = sched_nextpid();
    child->uid = current_task->uid;
    child->gid = current_task->gid;
    child->sid = current_task->sid;
    child->pgid = current_task->pgid;
    child->tgid = child->pid;

    
    child->name = strdup(current_task->name);
    child->description = strdup(current_task->description);
    child->argv = args_dup(current_task->argv);
    child->environ = args_dup(current_task->environ);

    child->status = TASK_STATUS_READY;
    child->priority = current_task->priority;
    child->starttime = timer_getticks();


    memcpy((void*) &child->signal.s_handlers, (void*) &current_task->signal.s_handlers, sizeof(struct sigaction) * TASK_NSIG);
    memcpy((void*) &child->signal.s_mask, (void*) &current_task->signal.s_mask, sizeof(sigset_t));
    
    list_each(current_task->signal.s_queue, q)
        list_push(child->signal.s_queue, q);


    child->thread_area = task_fork_thread_area(current_task->thread_area);
    
    child->cwd = current_task->cwd;
    child->exe = current_task->exe;
    child->root = current_task->root;
    child->umask = current_task->umask;

    
    memcpy((void*) child->fd, (const void*) current_task->fd, sizeof(fd_t) * TASK_FD_COUNT);    
    //memcpy(&child->iostat, &current_task->iostat, sizeof(current_task->iostat));
    //memcpy(&child->clock, &current_task->clock, sizeof(struct tms));
    memcpy((void*) &child->rlimits, (void*) &current_task->rlimits, sizeof(struct rlimit) * RLIM_NLIMITS);
    memcpy((void*) &child->exit, (void*) &current_task->exit, sizeof(current_task->exit));
    memcpy(&child->__image, current_task->image, sizeof(child->__image));
    
    child->image = &child->__image;
    child->image->refcount = 1;

    fifo_init(&child->fifo, TASK_FIFOSZ, 0);
        

    child->context = (void*) kmalloc(sizeof(i386_task_context_t), GFP_KERNEL);
    KASSERT(child->context);


    CTX(child)->eip = (uintptr_t) &return_from_fork;
    CTX(child)->esp = 
    CTX(child)->ebp = (uintptr_t) context;
    CTX(child)->vmmpd = (uintptr_t) vmm_clone((volatile pdt_t*) CTX(current_task)->vmmpd, 1);


    char* sys_stack = (char*) kmalloc(CONFIG_STACK_SIZE, GFP_KERNEL);
    KASSERT(sys_stack);
    memcpy(sys_stack, current_task->sys_stack, CONFIG_STACK_SIZE);
    child->sys_stack = &sys_stack[CONFIG_STACK_SIZE];

    
    child->parent = (task_t*) current_task;
    child->next = (task_t*) task_queue;

    task_queue = child;


    context->eax = (uintptr_t) child;
    INTR_ON;
}


volatile task_t* task_clone(int (*fn) (void*), void* stack, int flags, void* arg) {
    INTR_OFF;

    if(unlikely(!stack))
        stack = (void*) ((uintptr_t) kvalloc((uintptr_t) CONFIG_STACK_SIZE, GFP_USER) + (uintptr_t) CONFIG_STACK_SIZE);

    uintptr_t* sptr = (uintptr_t*) stack;
    *--sptr = (uintptr_t) arg;
    *--sptr = (uintptr_t) &return_from_clone_and_exit;
    *--sptr = (uintptr_t) fn;

    

    volatile task_t* child = (volatile task_t*) kmalloc(sizeof(task_t), GFP_KERNEL);
    KASSERT(child);

    memset((void*) child, 0, sizeof(task_t));

    child->pid = sched_nextpid();
    child->uid = current_task->uid;
    child->gid = current_task->gid;
    child->sid = current_task->sid;
    child->pgid = current_task->pgid;
    child->tgid = current_task->tgid;

    
    child->name = strdup(current_task->name);
    child->description = strdup(current_task->description);
    child->argv = args_dup(current_task->argv);
    child->environ = args_dup(current_task->environ);

    child->status = TASK_STATUS_READY;
    child->priority = current_task->priority;
    child->starttime = timer_getticks();

    child->thread_area = task_fork_thread_area(current_task->thread_area);


    
    if(flags & CLONE_SIGHAND) {
        memcpy((void*) &child->signal.s_handlers, (void*) &current_task->signal.s_handlers, sizeof(struct sigaction) * TASK_NSIG);
        memcpy((void*) &child->signal.s_mask, (void*) &current_task->signal.s_mask, sizeof(sigset_t));
        
        list_each(current_task->signal.s_queue, q)
            list_push(child->signal.s_queue, q);
    }


    if(flags & CLONE_FILES)
        memcpy((void*) child->fd, (const void*) current_task->fd, sizeof(fd_t) * TASK_FD_COUNT);


    if(flags & CLONE_FS) {
        child->cwd = current_task->cwd;
        child->root = current_task->root;
        child->umask = current_task->umask;
    } else {
        child->cwd = kernel_task->cwd;
        child->root = kernel_task->root;
        child->umask = 022;
    }

    child->exe = current_task->exe;

    //memcpy(&child->iostat, &current_task->iostat, sizeof(current_task->iostat));
    //memcpy(&child->clock, &current_task->clock, sizeof(struct tms));
    memcpy((void*) &child->rlimits, (void*) &current_task->rlimits, sizeof(struct rlimit) * RLIM_NLIMITS);
    memcpy((void*) &child->exit, (void*) &current_task->exit, sizeof(current_task->exit));

    fifo_init(&child->fifo, TASK_FIFOSZ, 0);


    child->context = (void*) kmalloc(sizeof(i386_task_context_t), GFP_KERNEL);
    KASSERT(child->context);


    CTX(child)->eip = (uintptr_t) &return_from_clone;
    CTX(child)->esp = 
    CTX(child)->ebp = (uintptr_t) sptr;


    char* sys_stack = (char*) kmalloc(CONFIG_STACK_SIZE, GFP_KERNEL);
    KASSERT(sys_stack);
    memcpy(sys_stack, current_task->sys_stack, CONFIG_STACK_SIZE);
    child->sys_stack = &sys_stack[CONFIG_STACK_SIZE];


    if(flags & CLONE_VM) {
        CTX(child)->vmmpd = (uintptr_t) vmm_clone((volatile pdt_t*) CTX(current_task)->vmmpd, 0);
        child->image = current_task->image;
        child->image->refcount++;
    } else {
        CTX(child)->vmmpd = (uintptr_t) vmm_clone((volatile pdt_t*) CTX(current_task)->vmmpd, 1);
        
        memcpy(&child->__image, current_task->image, sizeof(child->__image));
        child->image = &child->__image;
        child->image->refcount = 1;
        child->vmsize += child->image->end - child->image->start;
    }

    if(flags & CLONE_PARENT)
        child->parent = (task_t*) current_task->parent;
    else
        child->parent = (task_t*) current_task;
    

    child->next = (task_t*) task_queue;
    task_queue = child;

    INTR_ON;
    return child;
}



volatile task_t* task_fork(void) {
    
    volatile task_t* r = NULL;
    __asm__ __volatile__ ("int 0x7F" : "=a"(r) : "a"(0));

    return r;
}


void task_yield(void) {
    __asm__ __volatile__ ("int 0x7F" : : "a"(1));
}

void task_switch(volatile task_t* prev_task, volatile task_t* new_task) {
    INTR_OFF;

    uintptr_t esp, ebp, eip;
    __asm__ __volatile__ ("mov eax, esp" : "=a"(esp));
    __asm__ __volatile__ ("mov eax, ebp" : "=a"(ebp));
    
    eip = read_eip();
    if(eip == 0) {
        irq_ack(0);
        sched_dosignals();
        return;
    }

    CTX(prev_task)->eip = eip;
    CTX(prev_task)->esp = esp;
    CTX(prev_task)->ebp = ebp;

    __asm__ __volatile__ (
        "fsave [%0]     \n"
        "frstor [%1]    \n"
        : : "r"(FPU(CTX(prev_task)->fpu)), "r"(FPU(CTX(new_task)->fpu))
    );
    
    
    
    eip = CTX(new_task)->eip;
    esp = CTX(new_task)->esp;
    ebp = CTX(new_task)->ebp;

    x86_intr_kernel_stack((uintptr_t) new_task->sys_stack);



    volatile pdt_t* pd = (volatile pdt_t*) CTX(new_task)->vmmpd;

#if CONFIG_VMM
    vmm_switch(pd);
#endif


#if 1
    __asm__ __volatile__ (
        "mov gs, %0         \n"
        : : "r"(new_task->thread_area * 8)
    );
#endif

    __asm__ __volatile__ (
        "cli                \n"
        "mov ebx, %0        \n"
        "mov esp, %1        \n"
        "mov ebp, %2        \n"
        "mov cr3, %3        \n"
        "xor eax, eax       \n"
        "jmp ebx            \n"
        : : "r"(eip), "r"(esp), "r"(ebp), "r"(pd->physaddr)
        : "ebx", "esp", "eax"
    );    
}

void task_release(volatile task_t* task) {
    KASSERT(task);
    KASSERT(task != kernel_task);

        
    int i;
    if(likely(task->argv)) {
        for(i = 0; task->argv[i]; i++)
            kfree(task->argv[i]);
            
        kfree(task->argv);
    }
        
    if(likely(task->environ)) {
        for(i = 0; task->environ[i]; i++)
            kfree(task->environ[i]);        
        
        kfree(task->environ);
    }


    if(task->image && task->image->start < CONFIG_KERNEL_BASE) {
        uintptr_t p = task->image->start;
        for(p &= ~(PAGE_SIZE - 1); p < task->image->end; p += PAGE_SIZE)
            unmap_page(p);
    }

    task_set_thread_area(task, NULL);
    //vmm_release((volatile pdt_t*) CTX(task)->vmmpd);
}


int task_set_thread_area(volatile task_t* tk, struct __user_desc* uinfo) {
    /* See kernel/arch/i386/intr.asm */
    extern uint64_t GDT32[8192];
    extern void gdt_load();

    if(uinfo == NULL) {
        if(!tk->thread_area)
            return errno = EINVAL, -1;

        if(tk->thread_area < 3)
            return 0;

        GDT32[tk->thread_area] = 0;
        return tk->thread_area = 2, 0;
    }

    int i;
    if((i = uinfo->entry_number) == -1) {
        for(i = 3; i < 8192; i++) {
            if(GDT32[i] != 0)
                continue;

            break;
        }

        if(i == 8192)
            return errno = ESRCH, -1;

        tk->thread_area =
        uinfo->entry_number = i;
    }


    uint8_t* p = (uint8_t*) &GDT32[i];
    uintptr_t lm = uinfo->limit;
    
    if(lm > 65536) {
        lm >>= 12;
        p[6] = 0xC0;
    } else
        p[6] = 0x40;


    /* Limit */
    p[0] = lm & 0xFF;
    p[1] = (lm >> 8) & 0xFF;
    p[6] |= (lm >> 16) & 0x0F;

    /* Base */
    p[2] = uinfo->base_addr & 0xFF;
    p[3] = (uinfo->base_addr >> 8) & 0xFF;
    p[4] = (uinfo->base_addr >> 16) & 0xFF;
    p[7] = (uinfo->base_addr >> 24) & 0xFF;

    /* Flags */
    p[5] = 0x92;

    gdt_load();
    return 0;
}


int task_fork_thread_area(int th_area) {
     /* See kernel/arch/i386/intr.asm */
    extern uint64_t GDT32[8192];
    extern void gdt_load();

    if(!th_area)
        return errno = EINVAL, -1;

    int i;
    for(i = 3; i < 8192; i++) {
        if(GDT32[i] != 0)
            continue;

        break;
    }

    if(i == 8192)
        return errno = ESRCH, -1;

    GDT32[i] = GDT32[th_area];
    return i;
}



int task_init(void) {
    static task_t __t;
    static i386_task_context_t __c;
    static char __sys_stack[CONFIG_STACK_SIZE];

    volatile task_t* t = task_queue = kernel_task = (volatile task_t*) &__t;

    KASSERT(t);
    memset((void*) t, 0, sizeof(task_t));

    t->pid = sched_nextpid();
    t->uid = TASK_ROOT_UID;
    t->gid = TASK_ROOT_GID;
    t->pgid = t->pid;
    t->tgid = t->pid;
    t->sid = 0;
    
    t->name = KERNEL_NAME;    
    t->status = TASK_STATUS_READY;
    t->priority = TASK_PRIO_REGULAR;
    

    int i;
    for(i = 0; i < TASK_FD_COUNT; i++)
        memset((void*) &t->fd[i], 0, sizeof(fd_t));
    

    t->context = &__c;
    t->sys_stack = &__sys_stack[CONFIG_STACK_SIZE];
    t->thread_area = 2; /* gs: 0x10 */
    CTX(t)->vmmpd = (uintptr_t) current_pdt;


    for(i = 0; i < RLIM_NLIMITS; i++)
        t->rlimits[i].rlim_cur =
        t->rlimits[i].rlim_max = RLIM_INFINITY;



    t->image = &t->__image;
    t->image->start = CONFIG_KERNEL_BASE;
    t->image->end = (uintptr_t) &end;
    t->image->refcount = 1;
    t->vmsize += t->image->end - t->image->start;


    //x86_intr_kernel_stack((uintptr_t) t->sys_stack);

    current_task = t;
    return 0;
}
