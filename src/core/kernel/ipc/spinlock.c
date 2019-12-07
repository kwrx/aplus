#include <stdint.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>
#include <aplus/core/ipc.h>


/*!
 * @brief Initialize Spinlock.
 */
void spinlock_init(spinlock_t* lock) {
    DEBUG_ASSERT(lock);

    __atomic_clear(lock, __ATOMIC_RELAXED);
}

/*!
 * @brief Lock a Spinlock.
 */
void spinlock_lock(spinlock_t* lock) {
    DEBUG_ASSERT(lock);

    while(__atomic_test_and_set(lock, __ATOMIC_ACQUIRE))
#if defined(__i386__) || defined(__x86_64__)
        __builtin_ia32_pause();
#endif
        ;
}


/*!
 * @brief Try to lock a Spinlock.
 */
spinlock_t spinlock_trylock(spinlock_t* lock) {
    DEBUG_ASSERT(lock);

    return !__atomic_test_and_set(lock, __ATOMIC_ACQUIRE);
}


/*!
 * @brief Release a Spinlock.
 */
void spinlock_unlock(spinlock_t* lock) {
    DEBUG_ASSERT(lock);

    __atomic_clear(lock, __ATOMIC_RELEASE);
}