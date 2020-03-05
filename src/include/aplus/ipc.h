#ifndef _APLUS_IPC_H
#define _APLUS_IPC_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus.h>
#include <aplus/debug.h>

#define SPINLOCK_INIT       { 0L, 0L }

typedef volatile struct {
    volatile long value;
    volatile long flags;
} spinlock_t;




__BEGIN_DECLS

void spinlock_init(spinlock_t*);
void spinlock_lock(spinlock_t*);
void spinlock_unlock(spinlock_t*);
int spinlock_trylock(spinlock_t*);

__END_DECLS


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