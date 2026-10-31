/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
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

#include <fcntl.h>
#include <stdint.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>



typedef struct vfs_lock {

    inode_t* inode;
    uintptr_t owner;
    pid_t pid;
    short type;
    off_t start;
    off_t end;

    struct vfs_lock* next;

} vfs_lock_t;


static vfs_lock_t* vfs_locks = NULL;
static spinlock_t vfs_locks_lock;

volatile uint32_t vfs_lock_event = 0;



void vfs_locks_init(void) {

    spinlock_init(&vfs_locks_lock);
}


static inline bool __vfs_lock_overlaps(const vfs_lock_t* lock, off_t start, off_t end) {

    return lock->start <= end && start <= lock->end;
}


static vfs_lock_t* __vfs_lock_conflict(inode_t* inode, uintptr_t owner, short type, off_t start, off_t end) {

    for (vfs_lock_t* lock = vfs_locks; lock; lock = lock->next) {

        if (lock->inode != inode || lock->owner == owner)
            continue;

        if (!__vfs_lock_overlaps(lock, start, end))
            continue;

        if (type == F_WRLCK || lock->type == F_WRLCK)
            return lock;
    }

    return NULL;
}


static void __vfs_lock_apply(inode_t* inode, uintptr_t owner, pid_t pid, short type, off_t start, off_t end, vfs_lock_t** spare, vfs_lock_t** garbage) {

    bool merged = true;

    while (merged) {

        merged = false;

        for (vfs_lock_t** p = &vfs_locks; *p;) {

            vfs_lock_t* lock = *p;

            if (lock->inode != inode || lock->owner != owner) {
                p = &lock->next;
                continue;
            }

            const bool touches = (lock->end != VFS_LOCK_EOF && lock->end + 1 == start) || (end != VFS_LOCK_EOF && end + 1 == lock->start);

            if (type != F_UNLCK && lock->type == type && (touches || __vfs_lock_overlaps(lock, start, end))) {

                start = MIN(start, lock->start);
                end   = MAX(end, lock->end);

                *p         = lock->next;
                lock->next = *garbage;
                *garbage   = lock;

                merged = true;
                continue;
            }

            if (!__vfs_lock_overlaps(lock, start, end)) {
                p = &lock->next;
                continue;
            }

            if (lock->start < start && lock->end > end) {

                vfs_lock_t* right = spare[1] ? spare[1] : spare[0];

                if (right == spare[1])
                    spare[1] = NULL;
                else
                    spare[0] = NULL;

                *right       = *lock;
                right->start = end + 1;
                lock->end    = start - 1;
                lock->next   = right;

                p = &right->next;
                continue;
            }

            if (lock->start < start) {
                lock->end = start - 1;
                p         = &lock->next;
                continue;
            }

            if (lock->end > end) {
                lock->start = end + 1;
                p           = &lock->next;
                continue;
            }

            *p         = lock->next;
            lock->next = *garbage;
            *garbage   = lock;
        }
    }


    if (type == F_UNLCK)
        return;

    vfs_lock_t* lock = spare[0] ? spare[0] : spare[1];

    if (lock == spare[0])
        spare[0] = NULL;
    else
        spare[1] = NULL;

    lock->inode = inode;
    lock->owner = owner;
    lock->pid   = pid;
    lock->type  = type;
    lock->start = start;
    lock->end   = end;
    lock->next  = vfs_locks;

    vfs_locks = lock;
}


static void __vfs_lock_free(vfs_lock_t* lock) {

    while (lock) {

        vfs_lock_t* next = lock->next;
        kfree(lock);

        lock = next;
    }
}


void vfs_lock_test(inode_t* inode, uintptr_t owner, short* type, off_t* start, off_t* end, pid_t* pid) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(type);
    DEBUG_ASSERT(start);
    DEBUG_ASSERT(end);
    DEBUG_ASSERT(pid);


    scoped_lock(&vfs_locks_lock) {

        vfs_lock_t* lock = __vfs_lock_conflict(inode, owner, *type, *start, *end);

        if (lock) {

            *type  = lock->type;
            *start = lock->start;
            *end   = lock->end;
            *pid   = lock->pid;

            return;
        }
    }

    *type = F_UNLCK;
}


int vfs_lock_set(inode_t* inode, uintptr_t owner, pid_t pid, short type, off_t start, off_t end) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(start <= end);


    vfs_lock_t* spare[2] = {
        (vfs_lock_t*)kcalloc(1, sizeof(vfs_lock_t), GFP_KERNEL),
        (vfs_lock_t*)kcalloc(1, sizeof(vfs_lock_t), GFP_KERNEL),
    };

    vfs_lock_t* garbage = NULL;
    int e               = ENOMEM;

    if (likely(spare[0] && spare[1])) {

        e = 0;

        scoped_lock(&vfs_locks_lock) {

            if (type != F_UNLCK && __vfs_lock_conflict(inode, owner, type, start, end)) {
                e = EAGAIN;
                break;
            }

            __vfs_lock_apply(inode, owner, pid, type, start, end, spare, &garbage);
        }
    }


    if (spare[0])
        kfree(spare[0]);

    if (spare[1])
        kfree(spare[1]);

    __vfs_lock_free(garbage);


    if (e)
        return errno = e, -1;

    atomic_fetch_add(&vfs_lock_event, 1);

    return 0;
}


void vfs_lock_release(inode_t* inode, uintptr_t owner) {

    DEBUG_ASSERT(inode);


    vfs_lock_t* garbage = NULL;

    scoped_lock(&vfs_locks_lock) {

        for (vfs_lock_t** p = &vfs_locks; *p;) {

            vfs_lock_t* lock = *p;

            if (lock->inode != inode || lock->owner != owner) {
                p = &lock->next;
                continue;
            }

            *p         = lock->next;
            lock->next = garbage;
            garbage    = lock;
        }
    }

    if (!garbage)
        return;

    __vfs_lock_free(garbage);

    atomic_fetch_add(&vfs_lock_event, 1);
}
