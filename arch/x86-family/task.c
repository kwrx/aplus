/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/syscall.h>
#include <aplus/task.h>

#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>
#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/fpu.h>
#include <arch/x86/intr.h>
#include <arch/x86/vmm.h>


extern inode_t __vfs_root;

struct pty;


#define FRAME(p) ((interrupt_frame_t*)(p)->frame)

#define WRITE_SP0(cpu, ptr) ((tss_t*)(cpu)->tss)->sp0 = (uintptr_t)(ptr)



/**
 * @brief Loads an address space on this CPU, or reloads the current task's when given NULL.
 *
 * @param address_space The address space, or NULL.
 */
void arch_task_switch_address_space(vmm_address_space_t* address_space) {
    x86_vmm_tlb_load(address_space ? address_space : current_task->address_space);
}


#if defined(__x86_64__)

/**
 * @brief The frame a signal handler starts on: the return address into sa_restorer, then the interrupted context and
 *        the signal's details, laid out as on Linux.
 */
struct sigframe {

    uintptr_t restorer;
    ucontext_t uc;
    siginfo_t info;
};

#else
    #error "i386: not supported"
#endif


/**
 * @brief The flags a context restored by rt_sigreturn(2) may carry; the rest are the kernel's to set.
 */
#define SIGFRAME_USER_FLAGS 0x40CD5UL

/**
 * @brief The bytes below a user stack pointer that code may still be using, which a signal frame must skip.
 */
#define SIGFRAME_RED_ZONE 128

/**
 * @brief The alignment of the FPU state in a signal frame, which XSAVE requires.
 */
#define SIGFRAME_FPU_ALIGNMENT 64


/**
 * @brief Asks whether every page of a range of userspace allows an access.
 *
 * @param start The first byte.
 * @param size How many bytes.
 * @param mode R_OK or W_OK.
 * @return true if the whole range lies in userspace and allows the access.
 */
static bool __sigframe_range_ok(uintptr_t start, size_t size, int mode) {

    if (unlikely(size == 0 || start + size < start || start + size > X86_MMU_USERSPACE_END))
        return false;

    for (uintptr_t p = start & ~(X86_MMU_PAGESIZE - 1); p < start + size; p += X86_MMU_PAGESIZE) {

        if (!uio_check(p, mode))
            return false;
    }

    return true;
}


/**
 * @brief Asks whether a user stack pointer lies on the current task's alternate signal stack.
 *
 * @param sp The stack pointer.
 * @return true if it does.
 */
static bool __sigframe_on_altstack(uintptr_t sp) {

    const stack_t* alt = &current_task->userspace.altstack;

    return alt->ss_size && sp > (uintptr_t)alt->ss_sp && sp - (uintptr_t)alt->ss_sp <= alt->ss_size;
}


/**
 * @brief Copies the context the current task resumes into a signal frame, rewinding a restartable syscall to run again.
 *
 * @param uc The context to fill in.
 * @param restart Whether the syscall syscall_interrupt() gave up is to be re-executed when the handler returns.
 */
static void __sigframe_save_context(ucontext_t* uc, bool restart) {

    interrupt_frame_t* frame = FRAME(current_cpu);
    greg_t* g                = uc->uc_mcontext.gregs;

    g[REG_R8]  = frame->r8;
    g[REG_R9]  = frame->r9;
    g[REG_R10] = frame->r10;
    g[REG_R11] = frame->r11;
    g[REG_R12] = frame->r12;
    g[REG_R13] = frame->r13;
    g[REG_R14] = frame->r14;
    g[REG_R15] = frame->r15;
    g[REG_RDI] = frame->di;
    g[REG_RSI] = frame->si;
    g[REG_RBP] = frame->bp;
    g[REG_RBX] = frame->bx;
    g[REG_RDX] = frame->dx;
    g[REG_RAX] = frame->ax;
    g[REG_RCX] = frame->cx;

    if (x86_intr_is_user_mode(frame)) {

        g[REG_RIP]    = frame->ip;
        g[REG_RSP]    = frame->sp;
        g[REG_EFL]    = frame->flags;
        g[REG_ERR]    = frame->errno;
        g[REG_TRAPNO] = frame->intno;

    } else {

        g[REG_RIP] = frame->cx;
        g[REG_RSP] = (uintptr_t)current_cpu->ustack;
        g[REG_EFL] = frame->r11;
    }

    if (restart) {

        g[REG_RIP] -= 2;
        g[REG_RAX] = current_task->syscall.interrupted - 1;
        g[REG_RDI] = current_task->syscall.param0;
        g[REG_RSI] = current_task->syscall.param1;
        g[REG_RDX] = current_task->syscall.param2;
        g[REG_R10] = current_task->syscall.param3;
        g[REG_R8]  = current_task->syscall.param4;
        g[REG_R9]  = current_task->syscall.param5;
    }

    g[REG_CSGSFS] = (greg_t)(USER_CS | 3) | ((greg_t)(USER_DS | 3) << 48);


    const stack_t* alt = &current_task->userspace.altstack;

    uc->uc_stack.ss_sp    = alt->ss_sp;
    uc->uc_stack.ss_size  = alt->ss_size;
    uc->uc_stack.ss_flags = alt->ss_size == 0 ? SS_DISABLE : __sigframe_on_altstack((uintptr_t)g[REG_RSP]) ? SS_ONSTACK : 0;

    memcpy(&uc->uc_sigmask, current_task->syscall.sigmask_valid ? &current_task->syscall.sigmask : &current_task->sigmask, sizeof(sigset_t));

    current_task->syscall.sigmask_valid = false;
}


/**
 * @brief Sets the current task up to run a signal handler on a frame pushed onto its stack, or onto its alternate
 *        stack under SA_ONSTACK.
 *
 * The frame carries the interrupted context, the signal mask to restore, the siginfo and the FPU state, so each
 * thread and each nested signal has its own and rt_sigreturn(2) restores whatever the handler left there.
 *
 * @param siginfo The signal being delivered.
 * @return 0, or -1 if the frame does not fit in writable user memory.
 */
int arch_task_prepare_to_signal(siginfo_t* siginfo) {

    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(current_task->sigfpu);

    DEBUG_ASSERT(siginfo);
    DEBUG_ASSERT(siginfo->si_signo > 0);
    DEBUG_ASSERT(siginfo->si_signo < _NSIG);


    struct ksigaction action;
    bool found = false;

    shared_ptr_nullable_access(current_task->sighand, sighand, {
        action = sighand->action[siginfo->si_signo];
        found  = true;
    });

    if (unlikely(!found))
        return -1;


    struct sigframe sf;

    memset(&sf, 0, sizeof(sf));

    __sigframe_save_context(&sf.uc, current_task->syscall.interrupted && (action.sa_flags & SA_RESTART));

    current_task->syscall.interrupted = 0;

    memcpy(&sf.info, siginfo, sizeof(siginfo_t));

    sf.restorer = (uintptr_t)action.sa_restorer;


    const stack_t* alt = &current_task->userspace.altstack;

    uintptr_t sp = (uintptr_t)sf.uc.uc_mcontext.gregs[REG_RSP] - SIGFRAME_RED_ZONE;
    bool onalt   = __sigframe_on_altstack(sp);

    if ((action.sa_flags & SA_ONSTACK) && alt->ss_size && !onalt) {

        sp    = (uintptr_t)alt->ss_sp + alt->ss_size;
        onalt = true;
    }

    uintptr_t fpu = (sp - fpu_size()) & ~((uintptr_t)SIGFRAME_FPU_ALIGNMENT - 1);
    uintptr_t top = ((fpu - sizeof(struct sigframe)) & ~(uintptr_t)15) - sizeof(uintptr_t);

    if (unlikely(onalt && top < (uintptr_t)alt->ss_sp))
        return -1;

    if (unlikely(top > sp || !__sigframe_range_ok(top, sp - top, W_OK)))
        return -1;


    sf.uc.uc_mcontext.fpregs = (fpregset_t)fpu;

    fpu_save(current_task->sigfpu);

    uio_lock(top, sp - top);
    memcpy((void*)top, &sf, sizeof(sf));
    memcpy((void*)fpu, current_task->sigfpu, fpu_size());
    uio_unlock(top, sp - top);


    interrupt_frame_t* frame = FRAME(current_cpu);

    frame->ip    = (uintptr_t)action.handler;
    frame->cs    = USER_CS | 3;
    frame->flags = 0x202;
    frame->sp    = top;
    frame->ss    = USER_DS | 3;
    frame->di    = siginfo->si_signo;
    frame->si    = top + offsetof(struct sigframe, info);
    frame->dx    = top + offsetof(struct sigframe, uc);
    frame->ax    = 0;


    sigset_t handler_mask;

    memset(&handler_mask, 0, sizeof(sigset_t));
    memcpy(&handler_mask, &action.sa_mask, sizeof(action.sa_mask));

    for (size_t i = 0; i < SIGSET_WORDS; i++) {
        handler_mask.__bits[i] |= current_task->sigmask.__bits[i];
    }

    if (!(action.sa_flags & SA_NODEFER)) {
        sigset_add(&handler_mask, siginfo->si_signo);
    }

    sched_sigmask(&handler_mask);

    return 0;
}


/**
 * @brief Resumes the context saved in the frame of the signal handler that is returning, with its mask and FPU state.
 *
 * The frame sits just above the user stack pointer, whose return address into sa_restorer the handler has popped.
 * Whatever the handler changed in the frame takes effect, except for the segments and the privileged flags.
 *
 * @param retval Receives the restored rax, which rt_sigreturn(2) hands back so that it survives.
 * @return 0, or -1 if the frame cannot be read or does not describe a user context.
 */
int arch_task_return_from_signal(long* retval) {

    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(current_task->sigfpu);
    DEBUG_ASSERT(retval);


    uintptr_t uaddr = (uintptr_t)current_cpu->ustack - sizeof(uintptr_t) + offsetof(struct sigframe, uc);

    if (unlikely(!__sigframe_range_ok(uaddr, sizeof(ucontext_t), R_OK)))
        return -1;


    ucontext_t uc;

    uio_lock(uaddr, sizeof(ucontext_t));
    memcpy(&uc, (const void*)uaddr, sizeof(ucontext_t));
    uio_unlock(uaddr, sizeof(ucontext_t));


    const greg_t* g = uc.uc_mcontext.gregs;

    if (unlikely((uint64_t)g[REG_RIP] >= X86_MMU_USERSPACE_END || (uint64_t)g[REG_RSP] >= X86_MMU_USERSPACE_END))
        return -1;

    if (uc.uc_mcontext.fpregs) {

        uintptr_t fpu = (uintptr_t)uc.uc_mcontext.fpregs;

        if (unlikely(!__sigframe_range_ok(fpu, fpu_size(), R_OK)))
            return -1;

        uio_lock(fpu, fpu_size());
        memcpy(current_task->sigfpu, (const void*)fpu, fpu_size());
        uio_unlock(fpu, fpu_size());

        fpu_sanitize(current_task->sigfpu);
        fpu_restore(current_task->sigfpu);
    }


    interrupt_frame_t* frame = FRAME(current_cpu);

    frame->r8    = g[REG_R8];
    frame->r9    = g[REG_R9];
    frame->r10   = g[REG_R10];
    frame->r11   = g[REG_R11];
    frame->r12   = g[REG_R12];
    frame->r13   = g[REG_R13];
    frame->r14   = g[REG_R14];
    frame->r15   = g[REG_R15];
    frame->di    = g[REG_RDI];
    frame->si    = g[REG_RSI];
    frame->bp    = g[REG_RBP];
    frame->bx    = g[REG_RBX];
    frame->dx    = g[REG_RDX];
    frame->ax    = g[REG_RAX];
    frame->cx    = g[REG_RCX];
    frame->ip    = g[REG_RIP];
    frame->sp    = g[REG_RSP];
    frame->flags = ((uint64_t)g[REG_EFL] & SIGFRAME_USER_FLAGS) | 0x202;
    frame->cs    = USER_CS | 3;
    frame->ss    = USER_DS | 3;

    sched_sigmask(&uc.uc_sigmask);

    *retval = (long)frame->ax;

    return 0;
}



void arch_task_switch(task_t* prev, task_t* next) {

    DEBUG_ASSERT(current_cpu->frame);

    DEBUG_ASSERT(prev);
    DEBUG_ASSERT(prev->frame);

    DEBUG_ASSERT(next);
    DEBUG_ASSERT(next->frame);
    DEBUG_ASSERT(next->address_space->pm);



    if (unlikely(next->flags & TASK_FLAGS_NO_FRAME)) {

        memcpy(next->frame, current_cpu->frame, sizeof(interrupt_frame_t));
        next->flags &= ~TASK_FLAGS_NO_FRAME;

        fpu_save(next->fpu);
    }



    if (likely(prev != next)) {

        memcpy(prev->frame, current_cpu->frame, sizeof(interrupt_frame_t));

#if defined(__x86_64__)
        prev->userspace.thread_area = x86_rdmsr(X86_MSR_FSBASE);
#endif

        memcpy(current_cpu->frame, next->frame, sizeof(interrupt_frame_t));

        prev->kstack = current_cpu->kstack;
        prev->ustack = current_cpu->ustack;

        current_cpu->kstack = next->kstack;
        current_cpu->ustack = next->ustack;

        fpu_switch(prev->fpu, next->fpu);
    }

    x86_vmm_tlb_sync(next->address_space);



    WRITE_SP0(current_cpu, next->kstack);

#if defined(__x86_64__)
    x86_wrmsr(X86_MSR_FSBASE, next->userspace.thread_area);
#endif


    // // uint32_t m;

    // // #if TASK_SCHEDULER_PERIOD_NS != 1000000
    // //     m = (20LL - next->priority) / (TASK_SCHEDULER_PERIOD_NS / 1000000);
    // //     m = m ? m : 1;
    // // #else
    // //     m = (20LL - next->priority);
    // // #endif

    uint32_t m = TASK_SCHEDULER_PERIOD_NS / 1000000;

    apic_timer_reset(m);
}



/**
 * @brief Reports the ticks since boot, in the USER_HZ units /proc reports times in.
 *
 * @return The tick count.
 */
static inline uint64_t arch_task_boot_ticks(void) {
    return arch_timer_generic_getms() / (1000 / TASK_USER_HZ);
}


task_t* arch_task_get_empty_thread(size_t stacksize) {


    task_t* task = (task_t*)kcalloc(1, (sizeof(task_t)) + stacksize, GFP_KERNEL);



    task->argv    = current_task->argv;
    task->environ = current_task->environ;

    task->tid = task->pid = sched_nextpid();
    task->pgrp            = current_task->pgrp;
    task->uid             = current_task->uid;
    task->euid            = current_task->euid;
    task->gid             = current_task->gid;
    task->egid            = current_task->egid;
    task->sid             = current_task->sid;

    task->status   = TASK_STATUS_READY;
    task->policy   = current_task->policy;
    task->priority = current_task->priority;
    task->caps     = current_task->caps;
    task->flags    = 0;

    CPU_ZERO(&task->affinity);
    CPU_OR(&task->affinity, &task->affinity, &current_task->affinity);


    queue_init(&task->sigqueue);
    queue_init(&task->sigpending);

    memcpy(&task->sigmask, &current_task->sigmask, sizeof(sigset_t));


    task->ctty = shared_ptr_new(struct pty*, GFP_KERNEL);



#define _(size, offset) (void*)((uintptr_t)kcalloc(1, size, GFP_KERNEL) + offset)


    task->frame  = _(sizeof(interrupt_frame_t), 0);
    task->sigfpu = fpu_new_signal_state();
    task->kstack = _(KERNEL_SYSCALL_STACKSIZE, KERNEL_SYSCALL_STACKSIZE);
    task->ustack = NULL;
    task->fpu    = fpu_new_state();

#undef _


    FRAME(task)->cs    = KERNEL_CS;
    FRAME(task)->flags = 0x202;
    FRAME(task)->sp    = (((uintptr_t)task + sizeof(task_t) + stacksize) & ~0xFUL) - sizeof(uintptr_t);
    FRAME(task)->ss    = KERNEL_DS;


    spinlock_init(&task->lock);
    spinlock_init(&task->sched_lock);


    task->next = NULL;

    task->start_time = arch_task_boot_ticks();
    task->ppid       = current_task->pid;

    memcpy(task->comm, current_task->comm, TASK_COMM_LEN);
    memcpy(task->cmdline, current_task->cmdline, TASK_CMDLINE_LEN);

    task->cmdline_len = current_task->cmdline_len;

    return task;
}


pid_t arch_task_spawn_init() {

    task_t* task = (task_t*)kcalloc(1, (sizeof(task_t)), GFP_KERNEL);


    static char* __argv[2] = {"init", NULL};
    static char* __envp[1] = {NULL};

    task->argv    = __argv;
    task->environ = __envp;


    task->tid = task->pid = sched_nextpid();
    task->pgrp            = 1;
    task->uid = task->euid = task->gid = task->egid = 0;
    task->sid                                       = 1;

    task->status   = TASK_STATUS_READY;
    task->policy   = TASK_POLICY_RR;
    task->priority = TASK_PRIO_REG;
    task->caps     = TASK_CAPS_SYSTEM;
    task->flags    = TASK_FLAGS_NO_FRAME | TASK_FLAGS_KERNEL_UIO;

    CPU_ZERO(&task->affinity);
    CPU_SET(current_cpu->id, &task->affinity);


    queue_init(&task->sigqueue);
    queue_init(&task->sigpending);



#define _(size, offset) (void*)((uintptr_t)kcalloc(1, size, GFP_KERNEL) + offset)

    task->frame  = _(sizeof(interrupt_frame_t), 0);
    task->sigfpu = fpu_new_signal_state();
    task->kstack = _(KERNEL_SYSCALL_STACKSIZE, KERNEL_SYSCALL_STACKSIZE);
    task->ustack = NULL;
    task->fpu    = fpu_new_state();

#undef _


    task->address_space = &core->bsp.address_space;
    atomic_fetch_add(&core->bsp.address_space.refcount, 1);


    task->address_space->mmap.heap_start = KERNEL_MMAP_AREA;
    task->address_space->mmap.heap_end   = KERNEL_MMAP_AREA;
    task->address_space->mmap.heap_limit = KERNEL_MMAP_AREA + KERNEL_MMAP_SIZE;

    memset(&task->address_space->mmap.mappings, 0, sizeof(mmap_mapping_t) * CONFIG_MMAP_MAX);



    task->fs      = shared_ptr_new(struct fs, GFP_KERNEL);
    task->fd      = shared_ptr_new(struct fd, GFP_KERNEL);
    task->sighand = shared_ptr_new(struct sighand, GFP_KERNEL);
    task->ctty    = shared_ptr_new(struct pty*, GFP_KERNEL);


    shared_ptr_access(task->fs, fs, {
        fs->cwd   = vfs_inode_get(&__vfs_root);
        fs->root  = vfs_inode_get(&__vfs_root);
        fs->exe   = NULL;
        fs->umask = 0;
    });

    memset(&task->sigmask, 0xFF, sizeof(sigset_t));



    for (size_t j = 0; j < RLIM_NLIMITS; j++) {
        task->rlimits[j].rlim_cur = task->rlimits[j].rlim_max = RLIM_INFINITY;
    }

    task->rlimits[RLIMIT_STACK].rlim_cur = KERNEL_STACK_SIZE;



    task->next = NULL;

    task->start_time = arch_task_boot_ticks();
    task->ppid       = 0;

    strncpy(task->comm, "init", TASK_COMM_LEN - 1);

    memcpy(task->cmdline, "init", sizeof("init"));
    task->cmdline_len = sizeof("init");

    spinlock_init(&task->lock);
    spinlock_init(&task->sched_lock);



    current_cpu->sched_running = task;
    current_cpu->kstack        = task->kstack;

    WRITE_SP0(current_cpu, task->kstack);



#if DEBUG_LEVEL_TRACE
    kprintf("task: spawn init process pid(%d) cpu(%ld) kstack(%p)\n", task->tid, arch_cpu_get_current_id(), task->kstack);
#endif



    sched_enqueue(task);

    return task->tid;
}


/**
 * @brief Creates the idle task of the current cpu and installs it as its sched_idle.
 *
 * A cpu that is not running a task yet adopts the context it is already executing on, the way the boot
 * task does; one that is already running a task gets an idle task with a frame and a stack of its own.
 * The task is never placed on a run queue, so nothing that walks one can see or touch it.
 *
 * @return The idle task of the current cpu.
 */
task_t* arch_task_spawn_idle(void) {

    DEBUG_ASSERT(!current_cpu->sched_idle);


    const bool adopt = (current_cpu->sched_running == NULL);


    task_t* task = (task_t*)kcalloc(1, (sizeof(task_t)), GFP_KERNEL);


    static char* __argv[2] = {"idle", NULL};
    static char* __envp[1] = {NULL};

    task->argv    = __argv;
    task->environ = __envp;


    task->tid = task->pid = 0;
    task->pgrp            = 0;
    task->uid = task->euid = task->gid = task->egid = 0;
    task->sid                                       = 0;

    task->status   = TASK_STATUS_READY;
    task->policy   = TASK_POLICY_IDLE;
    task->priority = TASK_PRIO_MIN;
    task->caps     = TASK_CAPS_SYSTEM;
    task->flags    = TASK_FLAGS_KERNEL_UIO | (adopt ? TASK_FLAGS_NO_FRAME : 0);

    CPU_ZERO(&task->affinity);
    CPU_SET(current_cpu->id, &task->affinity);


    queue_init(&task->sigqueue);
    queue_init(&task->sigpending);



#define _(size, offset) (void*)((uintptr_t)kcalloc(1, size, GFP_KERNEL) + offset)

    task->frame  = _(sizeof(interrupt_frame_t), 0);
    task->sigfpu = fpu_new_signal_state();
    task->kstack = _(KERNEL_SYSCALL_STACKSIZE, KERNEL_SYSCALL_STACKSIZE);
    task->ustack = NULL;
    task->fpu    = fpu_new_state();

#undef _


    if (!adopt) {

        uintptr_t stack = (uintptr_t)kcalloc(1, KERNEL_SYSCALL_STACKSIZE, GFP_KERNEL);

        FRAME(task)->cs    = KERNEL_CS;
        FRAME(task)->ss    = KERNEL_DS;
        FRAME(task)->flags = 0x202;
        FRAME(task)->sp    = ((stack + KERNEL_SYSCALL_STACKSIZE) & ~0xFUL) - sizeof(uintptr_t);
        FRAME(task)->ip    = (uintptr_t)idle_main;
    }


    task->address_space = &core->bsp.address_space;
    atomic_fetch_add(&core->bsp.address_space.refcount, 1);


    task->fs      = shared_ptr_new(struct fs, GFP_KERNEL);
    task->fd      = shared_ptr_new(struct fd, GFP_KERNEL);
    task->sighand = shared_ptr_new(struct sighand, GFP_KERNEL);
    task->ctty    = shared_ptr_new(struct pty*, GFP_KERNEL);


    shared_ptr_access(task->fs, fs, {
        fs->cwd   = vfs_inode_get(&__vfs_root);
        fs->root  = vfs_inode_get(&__vfs_root);
        fs->exe   = NULL;
        fs->umask = 0;
    });

    memset(&task->sigmask, 0xFF, sizeof(sigset_t));



    for (size_t j = 0; j < RLIM_NLIMITS; j++) {
        task->rlimits[j].rlim_cur = task->rlimits[j].rlim_max = RLIM_INFINITY;
    }

    task->rlimits[RLIMIT_STACK].rlim_cur = KERNEL_STACK_SIZE;



    task->next = NULL;

    task->start_time = arch_task_boot_ticks();
    task->ppid       = 0;

    strncpy(task->comm, "idle", TASK_COMM_LEN - 1);

    memcpy(task->cmdline, "idle", sizeof("idle"));
    task->cmdline_len = sizeof("idle");

    spinlock_init(&task->lock);
    spinlock_init(&task->sched_lock);



    if (adopt) {

        current_cpu->sched_running = task;
        current_cpu->kstack        = task->kstack;

        WRITE_SP0(current_cpu, task->kstack);
    }

    current_cpu->sched_idle = task;



#if DEBUG_LEVEL_TRACE
    kprintf("task: spawn idle task cpu(%ld) kstack(%p) adopt(%d)\n", arch_cpu_get_current_id(), task->kstack, adopt);
#endif

    return task;
}


pid_t arch_task_spawn_kthread(const char* name, void (*entry)(void*), size_t stacksize, void* arg) {

    DEBUG_ASSERT(name);
    DEBUG_ASSERT(entry);
    DEBUG_ASSERT(stacksize);


    task_t* task = arch_task_get_empty_thread(stacksize);


    task->argv = (char**)kcalloc(1, (sizeof(char*) << 1) + strlen(name) + 1, GFP_KERNEL);

    task->argv[0] = (char*)&task->argv[2];
    task->argv[1] = (char*)NULL;

    strcpy(task->argv[0], name);


    CPU_ZERO(&task->affinity);

    for (size_t i = 0; i < CPU_SETSIZE; i++) {
        CPU_SET(i, &task->affinity);
    }

    task->pid    = current_task->tid;
    task->flags |= TASK_FLAGS_KTHREAD;



    task->address_space = (void*)current_task->address_space;

    atomic_fetch_add(&current_task->address_space->refcount, 1);


    shared_ptr_access(current_task->fs, fs, {
        task->fs = shared_ptr_dup(current_task->fs, GFP_KERNEL);
        fs_ref_all(fs);
    });

    task->fd      = shared_ptr_new(struct fd, GFP_KERNEL);
    task->sighand = shared_ptr_new(struct sighand, GFP_KERNEL);

    memset(&task->sigmask, 0xFF, sizeof(sigset_t));


    memcpy(&task->rlimits, &current_task->rlimits, sizeof(struct rlimit) * RLIM_NLIMITS);


    task->priority = TASK_PRIO_MIN;



    FRAME(task)->ip = (uintptr_t)entry;

#if defined(__x86_64__)
    FRAME(task)->di = (uintptr_t)arg;
#else
    FRAME(task)->sp -= sizeof(uintptr_t);
    *(uintptr_t*)FRAME(task)->sp = (uintptr_t)arg;

    FRAME(task)->sp -= sizeof(uintptr_t);
    *(uintptr_t*)FRAME(task)->sp = (uintptr_t)NULL;
#endif


    spinlock_init(&task->lock);
    spinlock_init(&task->sched_lock);


    task->next = NULL;

    strncpy(task->comm, name, TASK_COMM_LEN - 1);
    task->comm[TASK_COMM_LEN - 1] = '\0';

    task->cmdline_len = 0;


#if DEBUG_LEVEL_TRACE
    kprintf("task: spawn kthread %s pid(%d) ip(%p) kstack(%p) stacksize(%lX)\n", task->argv[0], task->tid, entry, task->kstack, stacksize);
#endif

    sched_enqueue(task);

    return task->tid;
}



/**
 * @brief Finds the frame a task resumes from: the cpu's own for the task running on it, its saved copy otherwise.
 *
 * @param task The task.
 * @return The frame.
 */
static interrupt_frame_t* __task_live_frame(task_t* task) {

    if (task == current_task && current_cpu->frame)
        return (interrupt_frame_t*)current_cpu->frame;

    return FRAME(task);
}


void arch_task_context_set(task_t* task, int options, long value) {

    DEBUG_ASSERT(task);
    DEBUG_ASSERT(task->frame);


    interrupt_frame_t* frame = __task_live_frame(task);


#if DEBUG_LEVEL_TRACE
    kprintf("task: set context tid(%d) options(%d) value(0x%lX)\n", task->tid, options, value);
#endif


    switch (options) {

        case ARCH_TASK_CONTEXT_COPY:
            memcpy(frame, (interrupt_frame_t*)value, sizeof(interrupt_frame_t));
            break;

        case ARCH_TASK_CONTEXT_PC:
            frame->ip = value;
            break;

        case ARCH_TASK_CONTEXT_STACK:
            frame->sp = value;
            break;

        case ARCH_TASK_CONTEXT_RETVAL:
            frame->ax = value;
            break;


#if defined(__i386__)

        case ARCH_TASK_CONTEXT_PARAM0:
        case ARCH_TASK_CONTEXT_PARAM1:
        case ARCH_TASK_CONTEXT_PARAM2:
        case ARCH_TASK_CONTEXT_PARAM3:
        case ARCH_TASK_CONTEXT_PARAM4:
        case ARCH_TASK_CONTEXT_PARAM5:

            frame->sp -= sizeof(long);
            uio_w32(frame->sp, value);
            break;

#endif


#if defined(__x86_64__)

        case ARCH_TASK_CONTEXT_PARAM0:
            frame->di = value;
            break;

        case ARCH_TASK_CONTEXT_PARAM1:
            frame->si = value;
            break;

        case ARCH_TASK_CONTEXT_PARAM2:
            frame->dx = value;
            break;

        case ARCH_TASK_CONTEXT_PARAM3:
            frame->cx = value;
            break;

        case ARCH_TASK_CONTEXT_PARAM4:
            frame->r8 = value;
            break;

        case ARCH_TASK_CONTEXT_PARAM5:
            frame->r9 = value;
            break;

#endif

        default:
            kpanicf("x86-task: PANIC! invalid ARCH_TASK_CONTEXT_* %d\n", options);
    }
}


long arch_task_context_get(task_t* task, int options) {

    DEBUG_ASSERT(task);
    DEBUG_ASSERT(task->frame);


    interrupt_frame_t* frame = __task_live_frame(task);


    switch (options) {

        case ARCH_TASK_CONTEXT_PC:
            return frame->ip;

        case ARCH_TASK_CONTEXT_STACK:
            return frame->sp;

        case ARCH_TASK_CONTEXT_RETVAL:
            return frame->ax;

        default:
            kpanicf("x86-task: PANIC! invalid ARCH_TASK_CONTEXT_* %d\n", options);
    }

    return -1;
}


/**
 * @brief Asks whether a task would resume inside the kernel, parked in the middle of a syscall.
 *
 * @param task The task.
 * @param live Whether its registers are the ones on the cpu right now rather than its saved copy.
 * @return true if the task is parked inside the kernel.
 */
bool arch_task_parked_in_kernel(const task_t* task, bool live) {

    DEBUG_ASSERT(task);


    interrupt_frame_t* frame = live ? (interrupt_frame_t*)current_cpu->frame : FRAME(task);

    if (unlikely(!frame))
        return false;

    if (x86_intr_is_user_mode(frame))
        return false;

#if defined(__x86_64__)
    extern uint8_t x86_syscall_leave[];

    return frame->ip != (uintptr_t)x86_syscall_leave;
#else
    return true;
#endif
}


/**
 * @brief Asks whether the current cpu is executing on a task's kernel stack.
 *
 * Only the kernel stack is checked: the stack a kernel thread runs on sits inside its task, and kernel threads
 * never exit.
 *
 * @param task The task.
 * @return true if the stack pointer lies within the task's kernel stack.
 */
bool arch_task_stack_in_use(const task_t* task) {

    DEBUG_ASSERT(task);

    uintptr_t sp  = (uintptr_t)__builtin_frame_address(0);
    uintptr_t top = (uintptr_t)task->kstack;

    return top && sp < top && sp >= top - KERNEL_SYSCALL_STACKSIZE;
}


/**
 * @brief Frees a task and everything it still owns: its stacks and frame, queued signals, futex registrations, and
 *        the argv of a kernel thread.
 *
 * @param task The task, which no cpu is running or executing on the stack of any more.
 */
void arch_task_destroy(task_t* task) {

    DEBUG_ASSERT(task);


    siginfo_t* siginfo;

    while ((siginfo = queue_pop(&task->sigqueue)) != NULL)
        kfree(siginfo);

    while ((siginfo = queue_pop(&task->sigpending)) != NULL)
        kfree(siginfo);

    if ((task->flags & TASK_FLAGS_KTHREAD) && task->argv)
        kfree(task->argv);

    if (task->frame) {
        kfree(task->frame);
    }

    if (task->fpu) {
        fpu_free_state(task->fpu);
    }

    if (task->sigfpu) {
        fpu_free_signal_state(task->sigfpu);
    }

    if (task->kstack) {
        kfree((void*)((uintptr_t)task->kstack - KERNEL_SYSCALL_STACKSIZE));
    }

    if (task->ctty) {
        shared_ptr_free(task->ctty);
    }

    futex_release_all(task);

    kfree(task);
}
