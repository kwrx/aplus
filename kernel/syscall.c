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


#define MAX_SYSCALL            1024

typedef long (*syscall_handler_t)
        (long, long, long, long, long);


static spinlock_t lck_syscall;
static syscall_handler_t __handlers[MAX_SYSCALL];

extern int syscalls_start;
extern int syscalls_end;




int syscall_init(void) {
    spinlock_init(&lck_syscall);

    memset(__handlers, 0, sizeof(__handlers));
    
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
    KASSERTF(!__handlers[number], "%d", number);

    __handlers[number] = (syscall_handler_t) handler;
    return 0;
}

int syscall_unregister(int number) {
    KASSERT(number < MAX_SYSCALL);

    __handlers[number] = (syscall_handler_t) NULL;
    return 0;
}


long syscall_handler(long number, long p0, long p1, long p2, long p3, long p4) {
    if(unlikely(number > MAX_SYSCALL))
        return errno = ENOSYS, -1;

    if(unlikely(!__handlers[number])) {
        kprintf(ERROR "syscall: called invalid syscall #%d (%p, %p, %p, %p, %p)\n", number, p0, p1, p2, p3, p4);
        return errno = ENOSYS, -1;
    }
    

    //spinlock_lock(&lck_syscall);
    if(unlikely(spinlock_trylock(&lck_syscall) != 0))
        kprintf(WARN "syscall: context locked for %d from %d\n", number, sys_getpid());

    INTR_ON;
    long r = __handlers[number] (p0, p1, p2, p3, p4);
    syscall_ack();

    return r;
}

void syscall_ack(void) {
    INTR_ON;
    spinlock_unlock(&lck_syscall);
}

EXPORT(syscall_register);
EXPORT(syscall_unregister);
