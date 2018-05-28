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
#include <aplus/debug.h>
#include <aplus/ipc.h>

#if CONFIG_IPC

int mutex_init(mutex_t* mtx, long kind, const char* name) {
    if(unlikely(!mtx))
        return -1;

    spinlock_init(&mtx->lock);
    mtx->recursion = 0;
    mtx->kind = kind;
    mtx->owner = -1;
    mtx->name = name;
    
    return 0;
}


int mutex_lock(mutex_t* mtx) {
    if(unlikely(!mtx))
        return -1;

    if(mtx->owner != sys_getpid()) {
#if CONFIG_IPC_DEBUG
        if(likely(mtx->name))
            kprintf(LOG "[%d] %s: LOCKED from %d\n", mtx->owner, mtx->name, sys_getpid());
#endif    
        spinlock_lock(&mtx->lock);

        mtx->owner = sys_getpid();
        mtx->recursion = 0;
    } else if(mtx->kind == MTX_KIND_ERRORCHECK) {
#if CONFIG_IPC_DEBUG
        if(likely(mtx->name))
            kprintf(ERROR "[%d] %s: DEADLOCK from %d\n", mtx->owner, mtx->name, sys_getpid());
#endif
        return -1;
    }

    if(mtx->kind == MTX_KIND_RECURSIVE)
        mtx->recursion += 1;

    return 0;
}

int mutex_trylock(mutex_t* mtx) {
    if(unlikely(!mtx))
        return -1;

    if(mtx->owner != sys_getpid()) {
        if(spinlock_trylock(&mtx->lock) == E_ERR)
            return -1;

        mtx->owner = sys_getpid();
        mtx->recursion = 0;
    } else if(mtx->kind == MTX_KIND_ERRORCHECK)
        return -1;

    if(mtx->kind == MTX_KIND_RECURSIVE)
        mtx->recursion += 1;

    return 0;
}

int mutex_unlock(mutex_t* mtx) {
    if(unlikely(!mtx))
        return -1;

    if(mtx->owner == sys_getpid()) {
        if(mtx->kind == MTX_KIND_RECURSIVE)
            if(--(mtx->recursion))
                return 0;

#if CONFIG_IPC_DEBUG
        if(likely(mtx->name))
            kprintf(LOG "[%d] %s: UNLOCKED from %d\n", mtx->owner, mtx->name, sys_getpid());
#endif
    
        spinlock_unlock(&mtx->lock);
        mtx->owner = -1;
    } else if(mtx->kind == MTX_KIND_ERRORCHECK)
        return -1;

    return 0;
}

EXPORT(mutex_init);
EXPORT(mutex_lock);
EXPORT(mutex_trylock);
EXPORT(mutex_unlock);

#endif
