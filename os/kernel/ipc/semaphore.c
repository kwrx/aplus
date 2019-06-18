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


void sem_init(semaphore_t* s, int value) {
    DEBUG_ASSERT(s);

    __atomic_store(s, &value, __ATOMIC_RELAXED);
}

void sem_wait(semaphore_t* s) {
    DEBUG_ASSERT(s);

    while(__atomic_load_n(s, __ATOMIC_CONSUME) > 0)
#if defined(__i386__) || defined(__x86_64__)
        __builtin_ia32_pause();
#endif
        ;

    __atomic_add_fetch(s, 1, __ATOMIC_RELEASE);
}

void sem_post(semaphore_t* s) {
    DEBUG_ASSERT(s);

    __atomic_sub_fetch(s, 1, __ATOMIC_ACQUIRE);
}