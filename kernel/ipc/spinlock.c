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
