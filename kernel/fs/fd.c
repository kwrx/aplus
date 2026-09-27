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
#include <aplus/task.h>
#include <aplus/vfs.h>


static struct file* filetable = NULL;
static spinlock_t filetable_lock;

static unsigned int lowestfree = 0;



void fd_init(void) {

    filetable  = (struct file*)kcalloc(CONFIG_FILE_MAX, sizeof(struct file), GFP_KERNEL);
    lowestfree = 0;

    spinlock_init(&filetable_lock);
}


struct file* fd_append(inode_t* inode, off_t position, int status) {

    DEBUG_ASSERT(filetable);
    DEBUG_ASSERT(inode);


    int i = CONFIG_FILE_MAX;

    scoped_lock(&filetable_lock) {
        for (i = lowestfree; i < CONFIG_FILE_MAX; i++) {

            if (filetable[i].refcount > 0)
                continue;


            lowestfree = i + 1;

            atomic_store(&filetable[i].refcount, 1);
            filetable[i].inode    = inode;
            filetable[i].position = position;
            filetable[i].status   = status;

            spinlock_init(&filetable[i].lock);
            break;
        }
    }


    if (i == CONFIG_FILE_MAX) {
        return errno = ENFILE, NULL;
    }

    return &filetable[i];
}


/**
 * @brief Drops a reference to an open file, releasing its slot and closing it outside filetable_lock on the last one.
 *
 * @param fd The file.
 * @param close Whether the last reference also closes the inode.
 */
void fd_remove(struct file* fd, bool close) {

    DEBUG_ASSERT(fd);
    DEBUG_ASSERT(filetable);


    inode_t* inode = NULL;

    scoped_lock(&filetable_lock) {
        if (atomic_fetch_sub(&fd->refcount, 1) == 1) {

            inode = fd->inode;

            fd->inode    = NULL;
            fd->position = 0;
            fd->status   = 0;


            int i = (int)(fd - filetable);

            DEBUG_ASSERT(i <= CONFIG_FILE_MAX - 1);
            DEBUG_ASSERT(i >= 0);

            if (i < lowestfree) {
                lowestfree = i;
            }
        }
    }


    if (!close || !inode)
        return;

    vfs_close(inode);

    if (inode->flags & INODE_FLAGS_ANONYMOUS)
        vfs_anonymous_free(inode);
}


/**
 * @brief Takes another reference to an open file the caller already holds one to.
 *
 * @param file The file.
 */
void fd_ref(struct file* file) {

    DEBUG_ASSERT(file);
    DEBUG_ASSERT(filetable);
    DEBUG_ASSERT(atomic_load(&file->refcount) > 0);

    atomic_fetch_add(&file->refcount, 1);
}


/**
 * @brief Takes a reference to the open file behind one of the caller's descriptors, leaving the table unlocked.
 *
 * @param fd The descriptor.
 * @param flags Receives the descriptor's flags, or NULL.
 * @return The file, or NULL if @p fd is not open.
 */
struct file* fd_get(unsigned int fd, int* flags) {

    DEBUG_ASSERT(current_task);


    if (unlikely(fd >= CONFIG_OPEN_MAX))
        return NULL;


    struct file* file = NULL;

    shared_ptr_access(current_task->fd, fds, {
        if (fds->descriptors[fd].ref != NULL) {

            file = fds->descriptors[fd].ref;

            if (flags)
                *flags = fds->descriptors[fd].flags;

            fd_ref(file);
        }
    });

    return file;
}


/**
 * @brief Drops a reference taken with fd_get().
 *
 * @param file The file.
 */
void fd_put(struct file* file) {

    fd_remove(file, true);
}


/**
 * @brief Closes every descriptor of a table that nothing references any more.
 *
 * @param fds The table.
 */
void fd_close_all(struct fd* fds) {

    DEBUG_ASSERT(fds);

    for (size_t i = 0; i < CONFIG_OPEN_MAX; i++) {

        if (!fds->descriptors[i].ref)
            continue;

        fd_remove(fds->descriptors[i].ref, true);

        fds->descriptors[i].ref   = NULL;
        fds->descriptors[i].flags = 0;
    }
}

