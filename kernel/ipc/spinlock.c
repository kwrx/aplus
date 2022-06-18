/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
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
                                                                        
#include <stdint.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/hal.h>


static inline uint64_t spinlock_get_new_owner(spinlock_t* lock) {

    DEBUG_ASSERT(lock);

    return (lock->flags & SPINLOCK_FLAGS_CPU_OWNER)
        ? (current_cpu  ? current_cpu->id   : 0)
        : (current_task ? current_task->tid : 0);

}



/*!
 * @brief Initialize Spinlock with flags.
 */
#if defined(DEBUG) && DEBUG_LEVEL >= 4
void __spinlock_init_with_flags(spinlock_t* lock, int flags, const char* FUNC, const char* FILE, int LINE) {
#else
void spinlock_init_with_flags(spinlock_t* lock, int flags) {
#endif

    DEBUG_ASSERT(lock);


    lock->flags    = flags;
    lock->owner    = -1ULL;
    lock->refcount = 0;
    lock->irqsave  = 0;

#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("ipc: spinlock_init_with_flags(%p, %d, %s, %s:%d)\n", lock, flags, FUNC, FILE, LINE);
#endif

    __atomic_clear(&lock->value, __ATOMIC_RELAXED);

}


/*!
 * @brief Initialize Spinlock.
 */
#if defined(DEBUG) && DEBUG_LEVEL >= 4
void __spinlock_init(spinlock_t* lock, const char* FUNC, const char* FILE, int LINE) {
#else
void spinlock_init(spinlock_t* lock) {
#endif

#if defined(DEBUG) && DEBUG_LEVEL >= 4
    return __spinlock_init_with_flags(lock, 0, FUNC, FILE, LINE);
#else
    return spinlock_init_with_flags(lock, 0);
#endif

}



/*!
 * @brief Lock a Spinlock.
 */
#if defined(DEBUG) && DEBUG_LEVEL >= 4
void __spinlock_lock(spinlock_t* lock, const char* FUNC, const char* FILE, int LINE) {
#else
void spinlock_lock(spinlock_t* lock) {
#endif

    DEBUG_ASSERT(lock);

        
    volatile uint64_t own;
    if((own = __atomic_load_n(&lock->owner, __ATOMIC_CONSUME)) == spinlock_get_new_owner(lock)) {

        if(lock->flags & SPINLOCK_FLAGS_RECURSIVE) {

            lock->refcount++;

        } else {
#if defined(DEBUG) && DEBUG_LEVEL >= 4
            kpanicf("ipc: PANIC! DEADLOCK! %s:%d %s(%p) owner(%ld) flags(%X) current_owner(%ld)\n", FILE, LINE, FUNC, lock, own, lock->flags, spinlock_get_new_owner(lock));
#else
            kpanicf("ipc: PANIC! DEADLOCK! owner(%ld) flags(%X)\n", own, lock->flags);
#endif
        }

    

    } else {


// // #if defined(DEBUG) && DEBUG_LEVEL >= 4
// //         uint64_t deadlock_detector = 0ULL;
// // #endif

        while(__atomic_test_and_set(&lock->value, __ATOMIC_ACQUIRE)) {

#if defined(__i386__) || defined(__x86_64__)
            __builtin_ia32_pause();
#endif

// // #if defined(DEBUG) && DEBUG_LEVEL >= 4
// //             if(deadlock_detector++ > (IPC_DEFAULT_TIMEOUT * 100000ULL)) {
// //                 kprintf("ipc: WARN! %s(): Timeout expired for %s:%d %s(%p), cpu(%d), tid(%d)\n", __func__, FILE, LINE, FUNC, lock, current_cpu->id, current_task->tid);
// //                 spinlock_unlock(lock);
// //                 deadlock_detector = 0ULL;
// //             }
// // #endif

        }

        DEBUG_ASSERT(lock->owner == -1ULL);

        lock->owner = spinlock_get_new_owner(lock);
        lock->irqsave = arch_intr_disable();
        lock->refcount = 1;

    }



}


/*!
 * @brief Try to lock a Spinlock.
 * @deprecated
 */
int spinlock_trylock(spinlock_t* lock) {

    DEBUG_ASSERT(lock);

    int e; 
    if((e = !__atomic_test_and_set(&lock->value, __ATOMIC_ACQUIRE)))
        lock->irqsave = arch_intr_disable();

    return e;
}


/*!
 * @brief Release a Spinlock.
 */
#if defined(DEBUG) && DEBUG_LEVEL >= 4
void __spinlock_unlock(spinlock_t* lock, const char* FUNC, const char* FILE, int LINE) {
#else
void spinlock_unlock(spinlock_t* lock) {
#endif


    DEBUG_ASSERT(lock);


    if(__atomic_load_n(&lock->owner, __ATOMIC_RELAXED) != spinlock_get_new_owner(lock)) {
#if defined(DEBUG) && DEBUG_LEVEL >= 4
        kpanicf("ipc: PANIC! EPERM! spinlock(%p) at %s:%d %s() not owned, owner(%ld) flags(%X) current_owner(%ld)\n", lock, FILE, LINE, FUNC, lock->owner, lock->flags, spinlock_get_new_owner(lock));
#else
        kpanicf("ipc: PANIC! EPERM! spinlock(%p) not owned, owner(%ld) flags(%X)\n", lock, lock->owner, lock->flags);
#endif

    } else {


        if(--lock->refcount > 0)
            return;


        long e = lock->irqsave;

        lock->owner    = -1ULL;
        lock->refcount = 0;
        lock->irqsave  = 0;

        __atomic_clear(&lock->value, __ATOMIC_RELEASE);


        arch_intr_enable(e);

    }
    
}
