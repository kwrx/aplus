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

#include <stdint.h>
#include <string.h>
#include <syscall.h>
#include <time.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/syscall.h>


#define SYSCALL_VERBOSE 0


#define SYSMAX      512
#define SYSATTEMPTS 3

long (*syscalls[SYSMAX])(long, long, long, long, long, long);

struct syscall_hook {

    uint32_t no;
    const void* ptr;
    const char* name;

#if defined(__x86_64__)
    char __padding[12];
#endif

} __packed;



void syscall_init(void) {


    extern uint8_t syscalls_start;
    extern uint8_t syscalls_end;

    memset(syscalls, 0, sizeof(syscalls));

    uintptr_t hook_start = (uintptr_t)&syscalls_start;
    uintptr_t hook_end   = (uintptr_t)&syscalls_end;


    for (; hook_start < hook_end; hook_start += sizeof(struct syscall_hook)) {

        struct syscall_hook* e = (struct syscall_hook*)(hook_start);

        DEBUG_ASSERT(e->no < SYSMAX);
        DEBUG_ASSERT(e->ptr);
        DEBUG_ASSERT(e->name);
        DEBUG_ASSERT(!syscalls[e->no]);

        syscalls[e->no] = e->ptr;
    }

#if DEBUG_LEVEL_INFO
    kprintf("syscall: registered %ld entries\n", ((uintptr_t)&syscalls_end - (uintptr_t)&syscalls_start) / sizeof(struct syscall_hook));
#endif
}



extern long sys_clock_gettime(clockid_t, struct timespec*);


long syscall_invoke(unsigned long idx, long p0, long p1, long p2, long p3, long p4, long p5) {

    if (unlikely(idx >= SYSMAX || !syscalls[idx]))
        return -ENOSYS;


#if DEBUG_LEVEL_TRACE && SYSCALL_VERBOSE
    if (unlikely(idx != 24 && idx < 500))
        kprintf("syscall: (%s#%d) <%s> nr(%ld), p0(0x%lX), p1(0x%lX), p2(0x%lX), p3(0x%lX), p4(0x%lX), p5(0x%lX)\n", current_task->argv[0], current_task->tid, runtime_get_name((uintptr_t)syscalls[idx]), idx, p0, p1, p2, p3, p4, p5);
#endif


    current_task->syscall.index  = idx + 1;
    current_task->syscall.param0 = p0;
    current_task->syscall.param1 = p1;
    current_task->syscall.param2 = p2;
    current_task->syscall.param3 = p3;
    current_task->syscall.param4 = p4;
    current_task->syscall.param5 = p5;



    errno = 0;


    long r = syscalls[idx](p0, p1, p2, p3, p4, p5);

    if (r < 0L)
        errno = -r;
    else
        errno = 0;



#if DEBUG_LEVEL_TRACE && SYSCALL_VERBOSE

    if (unlikely(idx != 24 && idx < 500)) {

        if (current_task->flags & TASK_FLAGS_NEED_RESCHED) {

            kprintf("syscall: (%s#%d) <%s> requested rescheduling, with possible return value (0x%lX)\n", current_task->argv[0], current_task->tid, runtime_get_name((uintptr_t)syscalls[idx]),
                    arch_task_context_get(current_task, ARCH_TASK_CONTEXT_RETVAL));
        }


        if (unlikely(errno == 0))
            kprintf("syscall: (%s#%d) <%s> return %ld\n", current_task->argv[0], current_task->tid, runtime_get_name((uintptr_t)syscalls[idx]), r);
        else
            kprintf("syscall: (%s#%d) <%s> ERROR! (%d) %s\n", current_task->argv[0], current_task->tid, runtime_get_name((uintptr_t)syscalls[idx]), errno, strerror(errno));
    }

#endif

    return r;
}


long syscall_restart(void) {

    if (unlikely(current_task->syscall.index == 0))
        return -ENOSYS;


#if DEBUG_LEVEL_TRACE && SYSCALL_VERBOSE
    kprintf("syscall: (%s#%d) <%s> restarting with p0(0x%lX), p1(0x%lX), p2(0x%lX), p3(0x%lX), p4(0x%lX), p5(0x%lX)\n", current_task->argv[0], current_task->tid, runtime_get_name((uintptr_t)syscalls[current_task->syscall.index - 1]),
            current_task->syscall.param0, current_task->syscall.param1, current_task->syscall.param2, current_task->syscall.param3, current_task->syscall.param4, current_task->syscall.param5);
#endif


    return syscall_invoke(current_task->syscall.index - 1, current_task->syscall.param0, current_task->syscall.param1, current_task->syscall.param2, current_task->syscall.param3, current_task->syscall.param4, current_task->syscall.param5);
}


/**
 * @brief Asks whether a syscall may be restarted after a signal handler with SA_RESTART, which as on Linux the waits
 *        for signals, time and descriptor readiness never are.
 *
 * @param idx The syscall number.
 * @return false for poll, select, pause, the sleeps, rt_sigsuspend, pselect6 and ppoll.
 */
static bool __syscall_restartable(long idx) {

    switch (idx) {

        case SYS_poll:
        case SYS_select:
        case SYS_pause:
        case SYS_nanosleep:
        case SYS_rt_sigsuspend:
        case SYS_clock_nanosleep:
        case SYS_pselect6:
        case SYS_ppoll:
            return false;

        default:
            return true;
    }
}


/**
 * @brief Gives up the syscall the current task is parked in, because a signal handler is about to run instead.
 *
 * The syscall is kept in syscall.interrupted for rt_sigreturn() to restart under SA_RESTART, unless it never
 * restarts, and whatever it carried across attempts is dropped. An interrupted sleep reports the time it had left.
 * A task that was not parked in a syscall has nothing to restart.
 */
void syscall_interrupt(void) {

    DEBUG_ASSERT(current_task);


    if (!(current_task->flags & TASK_FLAGS_NEED_SYSCALL_RESTART)) {

        current_task->syscall.interrupted.index = 0;
        return;
    }

    current_task->flags &= ~TASK_FLAGS_NEED_SYSCALL_RESTART;


    if (current_task->syscall.index > 0 && __syscall_restartable(current_task->syscall.index - 1)) {

        current_task->syscall.interrupted.index  = current_task->syscall.index;
        current_task->syscall.interrupted.param0 = current_task->syscall.param0;
        current_task->syscall.interrupted.param1 = current_task->syscall.param1;
        current_task->syscall.interrupted.param2 = current_task->syscall.param2;
        current_task->syscall.interrupted.param3 = current_task->syscall.param3;
        current_task->syscall.interrupted.param4 = current_task->syscall.param4;
        current_task->syscall.interrupted.param5 = current_task->syscall.param5;

    } else {

        current_task->syscall.interrupted.index = 0;
    }


    current_task->syscall.deadline_valid = false;


    if ((current_task->sleep.timeout.tv_sec || current_task->sleep.timeout.tv_nsec) && current_task->sleep.remaining) {

        struct timespec now = {0};

        long e = 0;

        scoped_uio_kernel() {
            e = sys_clock_gettime(current_task->sleep.clockid, &now);
        }

        if (likely(e == 0)) {

            uint64_t tss = (current_task->sleep.timeout.tv_sec * 1000000000ULL) + current_task->sleep.timeout.tv_nsec;
            uint64_t tsc = (now.tv_sec * 1000000000ULL) + now.tv_nsec;

            uint64_t left = tss > tsc ? tss - tsc : 0ULL;

            struct timespec remaining = {
                .tv_sec  = left / 1000000000ULL,
                .tv_nsec = left % 1000000000ULL,
            };

            uio_memcpy_s2u(current_task->sleep.remaining, &remaining, sizeof(remaining));
        }
    }

    current_task->sleep.timeout.tv_sec  = 0L;
    current_task->sleep.timeout.tv_nsec = 0L;
    current_task->sleep.remaining       = NULL;
    current_task->sleep.expired         = false;
}
