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
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/intr.h>
#include <libc.h>

extern int syscalls_start;
extern int syscalls_end;



typedef long (*syscall_handler_t)
        (long, long, long, long, long, long);

#define MAX_SYSCALL             1024
static syscall_handler_t syshandlers[MAX_SYSCALL];




static void syscall_error(long number, long p0, long p1, long p2, long p3, long p4, long p5, int err) {
    kprintf (ERROR
        "syscall: #%d (%p, %p, %p, %p, %p, %p): %s\n",
        number, p0, p1, p2, p3, p4, p5,
        strerror(err)
    );
}


int syscall_init(void) {
    memset(syshandlers, 0, sizeof(syshandlers));
    
    struct {
        int number;
        void* ptr;
        char* name;
    } *handler = (void*) &syscalls_start;

    for(
        handler = (void*) &syscalls_start;
        (uintptr_t) handler < (uintptr_t) &syscalls_end;
        handler++
    )
        syscall_register(handler->number, handler->ptr);
    


    return 0;
}


int syscall_register(int number, void* handler) {
    KASSERT(number < MAX_SYSCALL);
    KASSERTF(!syshandlers[number], "%d", number);

    syshandlers[number] = (syscall_handler_t) handler;
    return 0;
}

int syscall_unregister(int number) {
    KASSERT(number < MAX_SYSCALL);

    syshandlers[number] = (syscall_handler_t) NULL;
    return 0;
}


long syscall_handler(long number, long p0, long p1, long p2, long p3, long p4, long p5) {
    if(unlikely(number > MAX_SYSCALL))
        return syscall_error(number, p0, p1, p2, p3, p4, p5, ENOSYS), -ENOSYS;

    if(unlikely(!syshandlers[number]))
        return syscall_error(number, p0, p1, p2, p3, p4, p5, ENOSYS), -ENOSYS;
    

    errno = 0;
    long r = syshandlers[number] (p0, p1, p2, p3, p4, p5);

    if(unlikely(errno != 0)) {
        //syscall_error(number, p0, p1, p2, p3, p4, p5, errno);
        r = -errno;
    }

    INTR_ON;
    return r;
}

void syscall_ack(void) {
    INTR_ON;
}

EXPORT(syscall_register);
EXPORT(syscall_unregister);
