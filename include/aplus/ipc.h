/*                                                                      
 * Author(s):                                                           
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
                                                                        
#ifndef _APLUS_IPC_H
#define _APLUS_IPC_H

#ifndef __ASSEMBLY__

#include <aplus.h>
#include <aplus/debug.h>
#include <time.h>


#if defined(DEBUG)
#define IPC_DEFAULT_TIMEOUT     10000   //? 10sec
#endif


#define FUTEX_WAIT		                0
#define FUTEX_WAKE		                1
#define FUTEX_FD		                2
#define FUTEX_REQUEUE		            3
#define FUTEX_CMP_REQUEUE	            4
#define FUTEX_WAKE_OP		            5
#define FUTEX_LOCK_PI		            6
#define FUTEX_UNLOCK_PI		            7
#define FUTEX_TRYLOCK_PI	            8
#define FUTEX_WAIT_BITSET	            9
#define FUTEX_WAKE_BITSET	            10
#define FUTEX_WAIT_REQUEUE_PI	        11
#define FUTEX_CMP_REQUEUE_PI	        12

#define FUTEX_PRIVATE_FLAG	            128
#define FUTEX_CLOCK_REALTIME	        256
#define FUTEX_CMD_MASK		            ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)

#define FUTEX_WAITERS		            0x80000000
#define FUTEX_OWNER_DIED	            0x40000000
#define FUTEX_TID_MASK		            0x3FFFFFFF


#define FUTEX_WAIT_PRIVATE	            (FUTEX_WAIT | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_PRIVATE	            (FUTEX_WAKE | FUTEX_PRIVATE_FLAG)
#define FUTEX_REQUEUE_PRIVATE	        (FUTEX_REQUEUE | FUTEX_PRIVATE_FLAG)
#define FUTEX_CMP_REQUEUE_PRIVATE       (FUTEX_CMP_REQUEUE | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_OP_PRIVATE	        (FUTEX_WAKE_OP | FUTEX_PRIVATE_FLAG)
#define FUTEX_LOCK_PI_PRIVATE	        (FUTEX_LOCK_PI | FUTEX_PRIVATE_FLAG)
#define FUTEX_UNLOCK_PI_PRIVATE	        (FUTEX_UNLOCK_PI | FUTEX_PRIVATE_FLAG)
#define FUTEX_TRYLOCK_PI_PRIVATE        (FUTEX_TRYLOCK_PI | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAIT_BITSET_PRIVATE	    (FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_BITSET_PRIVATE	    (FUTEX_WAKE_BITSET | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAIT_REQUEUE_PI_PRIVATE	(FUTEX_WAIT_REQUEUE_PI | FUTEX_PRIVATE_FLAG)
#define FUTEX_CMP_REQUEUE_PI_PRIVATE	(FUTEX_CMP_REQUEUE_PI | FUTEX_PRIVATE_FLAG)


#define SPINLOCK_INIT                   { { 0L }, 0ULL, -1ULL, 0, 0 }
#define SPINLOCK_INIT_WITH_FLAGS(p)     { { 0L }, 0ULL, -1ULL, 0 | (p), 0 }
#define SPINLOCK_FLAGS_CPU_OWNER        (1 << 30)
#define SPINLOCK_FLAGS_RECURSIVE        (1 << 31)


#define SEMAPHORE_INIT                  0L




typedef volatile long semaphore_t;

typedef volatile struct {
    union {
        volatile char value;
        volatile uint64_t __padding;
    };

    volatile uint64_t irqsave;
    volatile uint64_t owner;

    volatile int flags;
    volatile int refcount;

} __packed spinlock_t;

typedef struct {

    volatile uint32_t* address;
    uint32_t value;

    struct timespec timeout;

} futex_t;







__BEGIN_DECLS

struct task;


#if defined(DEBUG) && DEBUG_LEVEL >= 4

void __spinlock_lock(spinlock_t*, const char*, const char*, int);
void __spinlock_unlock(spinlock_t*, const char*, const char*, int);
void __sem_wait(semaphore_t*, const char*, const char*, int);

#define spinlock_lock(spin) \
     __spinlock_lock(spin, __func__, __FILE__, __LINE__);
#define spinlock_unlock(spin) \
     __spinlock_unlock(spin, __func__, __FILE__, __LINE__);

#define sem_wait(sem)       \
    __sem_wait(sem, __func__, __FILE__, __LINE__);

#else

void spinlock_lock(spinlock_t*);
void spinlock_unlock(spinlock_t*);
void sem_wait(semaphore_t*);

#endif

void spinlock_init(spinlock_t*);
void spinlock_init_with_flags(spinlock_t*, int);
int spinlock_trylock(spinlock_t*);

void sem_init(semaphore_t*, long);
void sem_post(semaphore_t*);
int sem_trywait(semaphore_t* s);

void futex_rt_lock(void);
void futex_rt_unlock(void);
void futex_wait(struct task*, uint32_t*, uint32_t, const struct timespec*);
size_t futex_wakeup(uint32_t*, size_t);
size_t futex_requeue(uint32_t*, uint32_t*, size_t);
int futex_expired(futex_t*);

__END_DECLS


#define __lock_break    \
    break


#define __lock(lk, fn...)                   \
    {                                       \
        spinlock_lock((lk));                \
        do { fn; } while(0);                \
        spinlock_unlock((lk));              \
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
            kpanicf("spinlock: DEADLOCK! %s in %s:%d <%s>", #lk, __FILE__, __LINE__, __func__); \
        do { fn; } while(0);                    \
        spinlock_unlock((lk));                  \
    }

#define __trylock_return(lk, type, fn...)       \
    {                                           \
        type __r;                               \
        if(unlikely(!spinlock_trylock((lk))))   \
            kpanicf("spinlock: DEADLOCK! %s in %s:%d <%s>", #lk, __FILE__, __LINE__, __func__); \
        __r = fn;                               \
        spinlock_unlock((lk));                  \
        return __r;                             \
    }


#endif
#endif
