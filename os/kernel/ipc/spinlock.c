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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <stdint.h>


void spinlock_init(spinlock_t* lock) {
    __atomic_clear(lock, __ATOMIC_RELAXED);
}

void spinlock_lock(spinlock_t* lock) {
    while(__atomic_test_and_set(lock, __ATOMIC_ACQUIRE))
#if defined(__i386__) || defined(__x86_64__)
        __builtin_ia32_pause();
#endif
        ;
}

spinlock_t spinlock_trylock(spinlock_t* lock) {
    return !__atomic_test_and_set(lock, __ATOMIC_ACQUIRE);
}

void spinlock_unlock(spinlock_t* lock) {
    __atomic_clear(lock, __ATOMIC_RELEASE);
}
