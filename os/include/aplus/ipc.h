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


#ifndef _APLUS_IPC_H
#define _APLUS_IPC_H

typedef volatile int spinlock_t;
typedef volatile int semaphore_t;

#define IPC_DEFAULT_TIMEOUT                 10



#if defined(KERNEL)
#include <aplus.h>
#include <aplus/intr.h>

void spinlock_init(spinlock_t*);
void spinlock_lock(spinlock_t*);
void spinlock_unlock(spinlock_t*);
spinlock_t spinlock_trylock(spinlock_t*);

void sem_init(semaphore_t*, int);
void sem_wait(semaphore_t*);
void sem_post(semaphore_t*);
int sem_trywait(semaphore_t* s);


#define spinlock_irq_lock(lk)               \
    ({                                      \
        spinlock_lock((lk));                \
        long fl = arch_intr_disable();      \
        fl;                                 \
    })

#define spinlock_irq_unlock(lk, fl)         \
    {                                       \
        spinlock_unlock((lk));              \
        arch_intr_enable((fl));             \
    }

#define __lock(lk, fn...)                   \
    {                                       \
        spinlock_lock((lk));                \
        do { fn; } while(0);                \
        spinlock_unlock((lk));              \
    }

#define __lock_irq(lk, fn...)               \
    {                                       \
        long __f = spinlock_irq_lock((lk)); \
        do { fn; } while(0);                \
        spinlock_irq_unlock((lk), __f);     \
    }

#define __lock_return(lk, type, fn...)      \
    {                                       \
        type __r;                           \
        spinlock_lock(lk);                  \
        __r = fn;                           \
        spinlock_unlock(lk);                \
        return __r;                         \
    }

#define __trylock(lk, fn...)                    \
    {                                           \
        if(unlikely(!spinlock_trylock((lk))))   \
            kpanic("spinlock: DEADLOCK! %s in %s:%d <%s>", #lk, __FILE__, __LINE__, __func__); \
        do { fn; } while(0);                    \
        spinlock_unlock((lk));                  \
    }

#define __trylock_return(lk, type, fn...)       \
    {                                           \
        type __r;                               \
        if(unlikely(!spinlock_trylock((lk))))   \
            kpanic("spinlock: DEADLOCK! %s in %s:%d <%s>", #lk, __FILE__, __LINE__, __func__); \
        __r = fn;                               \
        spinlock_unlock((lk));                  \
        return __r;                             \
    }


#define atomic_load(ptr)                    \
    __atomic_load_n(ptr, __ATOMIC_ACQUIRE)

#define atomic_store(ptr, val)              \
    __atomic_load_n(ptr, val, __ATOMIC_RELEASE)

#define atomic_exchange(ptr, val)           \
    __atomic_exchange(ptr, val, __ATOMIC_ACQ_REL)

#define atomic_thread_fence()               \
    __atomic_thread_fence(__ATOMIC_SEQ_CST)

#define atomic_signal_fence()               \
    __atomic_signal_fence(__ATOMIC_SEQ_CST)


#endif

#endif