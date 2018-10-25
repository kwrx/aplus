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
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <aplus/intr.h>
#include <libc.h>

SYSCALL(1, exit,
void sys_exit(int status) {
    KASSERT(current_task != kernel_task);
    
    if(status & (1 << 31))
        current_task->exit.value = status & 0x7FFF;
    else
        current_task->exit.value = (status & 0377) << 8;    


#if DEBUG
    kprintf(INFO "exit: task %d (%s) %s with %04X (U: %0.3fs, C: %0.3fs, VM: %d)\n", 
        current_task->pid, 
        current_task->name,
        WIFSTOPPED(current_task->exit.value) ? "stopped" : "exited",
        current_task->exit.value & 0xFFFF,
        (double) current_task->clock.tms_utime / CLOCKS_PER_SEC,
        (double) current_task->clock.tms_cutime / CLOCKS_PER_SEC,
        current_task->image->refcount
    );   
#endif


    current_task->status = (WIFSTOPPED(current_task->exit.value))
        ? TASK_STATUS_STOP
        : TASK_STATUS_ZOMBIE;


    if(current_task->parent) {
        siginfo_t si;
        si.si_code = SI_KERNEL;
        si.si_pid = current_task->pid;
        si.si_uid = current_task->uid;
        si.si_value.sival_int = current_task->exit.value;

        sched_sigqueueinfo(current_task->parent, SIGCHLD, &si);
    }


    if(current_task->status == TASK_STATUS_STOP) {
        while(current_task->status == TASK_STATUS_STOP)
            sys_yield();

        return;
    }

    INTR_OFF;

    volatile task_t* tmp;
    for(tmp = task_queue; tmp; tmp = tmp->next)
        if(tmp->parent == current_task)
            tmp->parent = kernel_task;

    
    int i;
    for(i = 0; i < TASK_FD_COUNT; i++)
        sys_close(i);


    if((--current_task->image->refcount) == 0)
        task_release(current_task);

    syscall_ack();

    INTR_ON;
    for(;;) 
        sys_yield();
});
