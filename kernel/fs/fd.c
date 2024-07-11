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

#include <poll.h>
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>


static struct file* filetable = NULL;
static spinlock_t filetable_lock;

static unsigned int lowestfree = 0;



void fd_init(void) {

    filetable  = (struct file*)kcalloc(sizeof(struct file), CONFIG_FILE_MAX, GFP_KERNEL);
    lowestfree = 0;

    spinlock_init(&filetable_lock);
}


struct file* fd_append(inode_t* inode, off_t position, int status) {

    DEBUG_ASSERT(filetable);
    DEBUG_ASSERT(inode);


    int i = CONFIG_FILE_MAX;

    __lock(&filetable_lock, {
        for (i = lowestfree; i < CONFIG_FILE_MAX; i++) {

            if (filetable[i].refcount > 0)
                continue;


            lowestfree = i + 1;

            filetable[i].refcount = 1;
            break;
        }
    });


    if (i == CONFIG_FILE_MAX) {
        return errno = ENFILE, NULL;
    }



    filetable[i].inode    = inode;
    filetable[i].position = position;
    filetable[i].status   = status;

    spinlock_init(&filetable[i].lock);


    return &filetable[i];
}


void fd_remove(struct file* fd, bool close) {

    DEBUG_ASSERT(fd);
    DEBUG_ASSERT(filetable);


    __lock(&filetable_lock, {
        if (atomic_fetch_sub(&fd->refcount, 1) == 1) {

            if (close) {
                vfs_close(fd->inode);
            }

            fd->inode    = NULL;
            fd->position = 0;
            fd->status   = 0;


            unsigned int i = (int)((uintptr_t)fd - (uintptr_t)filetable) / sizeof(struct file);

            DEBUG_ASSERT(i <= CONFIG_FILE_MAX - 1);
            DEBUG_ASSERT(i >= 0);

            if (i < lowestfree) {
                lowestfree = i;
            }
        }
    });
}


void fd_ref(struct file* file) {

    DEBUG_ASSERT(file);
    DEBUG_ASSERT(filetable);

    __lock(&filetable_lock, { atomic_fetch_sub(&file->refcount, 1); });
}
