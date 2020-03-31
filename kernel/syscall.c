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
#include <aplus/syscall.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <stdint.h>
#include <string.h>



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



long syscall_invoke(long idx, long p0, long p1, long p2, long p3, long p4, long p5) {
    
    DEBUG_ASSERT(idx < SYSMAX);
    DEBUG_ASSERT(syscalls[idx]);



#if defined(DEBUG) && DEBUG_LEVEL >= 4
    if(unlikely(idx != 24))
        kprintf("syscall: (%s) <%s> nr(%d), p0(%p), p1(%p), p2(%p), p3(%p), p4(%p), p5(%p)\n", current_task->argv[0], runtime_get_name((uintptr_t) syscalls[idx]), idx, p0, p1, p2, p3, p4, p5);
#endif


    long r;
    if((r = syscalls[idx] (p0, p1, p2, p3, p4, p5)) < 0)
        errno = -r;
    else
        errno = 0;


#if defined(DEBUG) && DEBUG_LEVEL >= 0
    if(unlikely(errno == ENOSYS))
        kprintf("syscall: (%s) <%s> ENOSYS! nr(%d), p0(%p), p1(%p), p2(%p), p3(%p), p4(%p), p5(%p)\n", current_task->argv[0], runtime_get_name((uintptr_t) syscalls[idx]), idx, p0, p1, p2, p3, p4, p5);
#endif

#if defined(DEBUG) && DEBUG_LEVEL >= 2
    if(unlikely(errno == EFAULT))
        kprintf("syscall: (%s) <%s> EFAULT! nr(%d), p0(%p), p1(%p), p2(%p), p3(%p), p4(%p), p5(%p)\n", current_task->argv[0], runtime_get_name((uintptr_t) syscalls[idx]), idx, p0, p1, p2, p3, p4, p5);
#endif

    return r;
}