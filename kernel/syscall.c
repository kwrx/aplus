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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/errno.h>
#include <aplus/hal.h>




#define SYSMAX          512
#define SYSATTEMPTS     3

long (*syscalls[SYSMAX])
    (long, long, long, long, long, long);

struct syscall_hook {
    
    uint32_t no;
    const void* ptr;
    const char* name;

#if defined (__x86_64__)
        char __padding[12];
#endif

} __packed;



void syscall_init(void) {


    extern uint8_t syscalls_start;
    extern uint8_t syscalls_end;

    memset(syscalls, 0, sizeof(syscalls));

    uintptr_t hook_start = (uintptr_t) &syscalls_start;
    uintptr_t hook_end   = (uintptr_t) &syscalls_end;


    for(; hook_start < hook_end; hook_start += sizeof(struct syscall_hook)) {

        struct syscall_hook* e = (struct syscall_hook*) (hook_start);

        DEBUG_ASSERT(e->no < SYSMAX);
        DEBUG_ASSERT(e->ptr);
        DEBUG_ASSERT(e->name);
        DEBUG_ASSERT(!syscalls[e->no]);

        syscalls[e->no] = e->ptr;

    }

#if defined(DEBUG) && DEBUG_LEVEL >= 0
    kprintf("syscall: registered %ld entries\n", ((uintptr_t) &syscalls_end - (uintptr_t) &syscalls_start) / sizeof(struct syscall_hook));
#endif
}



long syscall_invoke(unsigned long idx, long p0, long p1, long p2, long p3, long p4, long p5) {
    
    DEBUG_ASSERT(idx < SYSMAX);
    DEBUG_ASSERT(syscalls[idx]);


#if defined(DEBUG) && DEBUG_LEVEL >= 4
    if(unlikely(idx != 24 && idx < 500))
        kprintf("syscall: (%s#%d) <%s> nr(%ld), p0(0x%lX), p1(0x%lX), p2(0x%lX), p3(0x%lX), p4(0x%lX), p5(0x%lX)\n", current_task->argv[0], current_task->tid, runtime_get_name((uintptr_t) syscalls[idx]), idx, p0, p1, p2, p3, p4, p5);
#endif


    current_task->syscall.index  = idx + 1;
    current_task->syscall.param0 = p0;
    current_task->syscall.param1 = p1;
    current_task->syscall.param2 = p2;
    current_task->syscall.param3 = p3;
    current_task->syscall.param4 = p4;
    current_task->syscall.param5 = p5;



    errno = 0;


    long r = syscalls[idx] (p0, p1, p2, p3, p4, p5);

    if(r < 0L)
        errno = -r;
    else
        errno = 0;
    
   


#if defined(DEBUG) && DEBUG_LEVEL >= 4

    if(unlikely(idx != 24 && idx < 500)) {

        if(current_task->flags & TASK_FLAGS_NEED_RESCHED) {

            kprintf("syscall: (%s#%d) <%s> requested rescheduling, with possible return value (0x%lX)\n", 
                current_task->argv[0], 
                current_task->tid, 
                runtime_get_name((uintptr_t) syscalls[idx]), 
                arch_task_context_get(current_task, ARCH_TASK_CONTEXT_RETVAL));

        }


        if(unlikely(errno == 0))
            kprintf("syscall: (%s#%d) <%s> return %ld\n", current_task->argv[0], current_task->tid, runtime_get_name((uintptr_t) syscalls[idx]), r);
        else
            kprintf("syscall: (%s#%d) <%s> ERROR! (%d) %s\n", current_task->argv[0], current_task->tid, runtime_get_name((uintptr_t) syscalls[idx]), errno, strerror(errno));
    
    }

#endif

    return r;
}


long syscall_restart(void) {

    if(unlikely(current_task->syscall.index == 0))
        return -ENOSYS;


#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("syscall: (%s#%d) <%s> restarting with p0(0x%lX), p1(0x%lX), p2(0x%lX), p3(0x%lX), p4(0x%lX), p5(0x%lX)\n", current_task->argv[0], current_task->tid, runtime_get_name((uintptr_t) syscalls[current_task->syscall.index - 1]),
        current_task->syscall.param0,
        current_task->syscall.param1,
        current_task->syscall.param2,
        current_task->syscall.param3,
        current_task->syscall.param4,
        current_task->syscall.param5
    );
#endif


    return syscall_invoke(current_task->syscall.index - 1,
                          current_task->syscall.param0,
                          current_task->syscall.param1,
                          current_task->syscall.param2,
                          current_task->syscall.param3,
                          current_task->syscall.param4,
                          current_task->syscall.param5);

}