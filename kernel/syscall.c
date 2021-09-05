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
#include <syscall.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/errno.h>
#include <aplus/hal.h>




#define SYSMAX 512

long (*syscalls[SYSMAX])
    (long, long, long, long, long, long);



void syscall_init(void) {


    extern int syscalls_start;
    extern int syscalls_end;

    memset(syscalls, 0, sizeof(syscalls));

    struct {

        uint32_t no;
        void* ptr;
        char* name;

#if defined (__x86_64__)
        char __padding[12];
#endif
    } __packed *e = (void*) &syscalls_start;


    for(; (uintptr_t) e < (uintptr_t) &syscalls_end; e++) {

        DEBUG_ASSERT(e->no < SYSMAX);
        DEBUG_ASSERT(e->ptr);
        DEBUG_ASSERT(e->name);
        DEBUG_ASSERT(!syscalls[e->no]);

        syscalls[e->no] = e->ptr;

    }

#if defined(DEBUG) && DEBUG_LEVEL >= 0
    kprintf("syscall: registered %d entries\n", ((uintptr_t) &syscalls_end - (uintptr_t) &syscalls_start) / sizeof(*e));
#endif
}



long syscall_invoke(unsigned long idx, long p0, long p1, long p2, long p3, long p4, long p5) {
    
    DEBUG_ASSERT(idx < SYSMAX);
    DEBUG_ASSERT(syscalls[idx]);


#if defined(DEBUG) && DEBUG_LEVEL >= 4
    if(unlikely(idx != 24))
        kprintf("syscall: (%s#%d) <%s> nr(%d), p0(%p), p1(%p), p2(%p), p3(%p), p4(%p), p5(%p)\n", current_task->argv[0], current_task->tid, runtime_get_name((uintptr_t) syscalls[idx]), idx, p0, p1, p2, p3, p4, p5);
#endif



    //* Restartable syscalls

    switch(idx) {

        // TODO
        // case SYS_read:
        // case SYS_readv:
        // case SYS_write:
        // case SYS_writev:
        // case SYS_ioctl:

        //     current_task->syscall.param0 = p0;
        //     current_task->syscall.param1 = p1;
        //     current_task->syscall.param2 = p2;
        //     current_task->syscall.param3 = p3;
        //     current_task->syscall.param4 = p4;
        //     current_task->syscall.param5 = p5;


        case SYS_nanosleep:
        case SYS_clock_nanosleep:
        case SYS_futex:
        case SYS_poll:
        case SYS_flock:
        case SYS_wait4:
        case SYS_waitid:

            current_task->syscall.index = idx + 1;
            break;


        default:
            break;

    }



    long r;
    if((r = syscalls[idx] (p0, p1, p2, p3, p4, p5)) < 0L)
        errno = -r;
    else
        errno = 0;




#if defined(DEBUG) && DEBUG_LEVEL >= 4

    if(unlikely(idx != 24)) {

        if(current_task->flags & TASK_FLAGS_NEED_RESCHED) {

            kprintf("syscall: (%s#%d) <%s> requested reschedule, with possible return value (%p)\n", 
                current_task->argv[0], 
                current_task->tid, 
                runtime_get_name((uintptr_t) syscalls[idx]), 
                arch_task_context_get(current_task, ARCH_TASK_CONTEXT_RETVAL));

        }


        if(unlikely(errno == 0))
            kprintf("syscall: (%s#%d) <%s> return %d\n", current_task->argv[0], current_task->tid, runtime_get_name((uintptr_t) syscalls[idx]), r);
        else
            kprintf("syscall: (%s#%d) <%s> ERROR! %s\n", current_task->argv[0], current_task->tid, runtime_get_name((uintptr_t) syscalls[idx]), strerror(errno));
    
    }

#endif

    return r;
}



long syscall_restart(void) {

    if(unlikely(current_task->syscall.index-- == 0))
        return -ENOSYS;


    long e = 0;

    switch(current_task->syscall.index) {

        case SYS_read:
        case SYS_readv:
        case SYS_write:
        case SYS_writev:
        case SYS_ioctl:

            e = syscall_invoke(current_task->syscall.index, current_task->syscall.param0,
                                                            current_task->syscall.param1,
                                                            current_task->syscall.param2,
                                                            current_task->syscall.param3,
                                                            current_task->syscall.param4,
                                                            current_task->syscall.param5);
            
            break;


        case SYS_nanosleep:
        case SYS_clock_nanosleep:
        case SYS_futex:
        case SYS_poll:
        case SYS_flock:
        case SYS_wait4:
        case SYS_waitid:

            thread_suspend(current_task);
            thread_postpone_resched(current_task);

            arch_task_context_set(current_task, ARCH_TASK_CONTEXT_RETVAL, 0L);
            break;

        default:
            e = -EINTR;

    }



#if defined(DEBUG) && DEBUG_LEVEL >= 4
    if(likely(e != -EINTR))
        kprintf("syscall: (%s#%d) <%s> restarted!\n", current_task->argv[0], current_task->tid, runtime_get_name((uintptr_t) syscalls[current_task->syscall.index]));
#endif



    current_task->syscall.index = 0;

    return e;

}
