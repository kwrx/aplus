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


#ifndef _APLUS_SMP_H
#define _APLUS_SMP_H

#if defined(KERNEL)
#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/timer.h>
#include <aplus/task.h>

#define CPU_MAX                     16
#define CPU_FLAGS_ENABLED           (1 << 0)
#define CPU_FLAGS_BSP               (1 << 1)
#define CPU_FLAGS_SCHED_ENABLED     (1 << 2)


typedef struct {
    int id;
    int flags;
    spinlock_t lock;

    struct {
        task_t core;
        task_t* current;
    } task;
    
    struct timespec ticks;
} cpu_t;


cpu_t* get_current_cpu(void);
cpu_t* get_cpu(int);


#define foreach_cpu(cpu)                \
    for(int i = 0; i < CPU_MAX; i++)    \
        if((cpu = get_cpu(i)) == NULL)  \
            {} else

#define current_cpu                     \
    get_current_cpu()

#define current_task                    \
    (current_cpu->task.current)


#endif
#endif