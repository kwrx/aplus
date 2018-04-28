#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <aplus/intr.h>
#include <aplus/timer.h>

#if CONFIG_IPC


int spinlock_init(spinlock_t* lock) {
    if(unlikely(!lock))
        return -1;

    *lock = 0;
    return 0;
}


void spinlock_lock(spinlock_t* lock) {
    ipc_wait(!__sync_bool_compare_and_swap(lock, 0, 1));
}

int spinlock_trylock(spinlock_t* lock) {
    if(unlikely(!__sync_bool_compare_and_swap(lock, 0, 1)))
        return -1;

    return 0;
}

void spinlock_unlock(spinlock_t* lock) {
    __sync_lock_release(lock);
}


EXPORT(spinlock_init);
EXPORT(spinlock_lock);
EXPORT(spinlock_trylock);
EXPORT(spinlock_unlock);

#endif
