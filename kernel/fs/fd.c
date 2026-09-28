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

#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/memory.h>
#include <aplus/task.h>
#include <aplus/vfs.h>


/**
 * @brief The open() flags an open file keeps: its access mode and its status flags.
 */
#define FILE_STATUS_FLAGS (O_ACCMODE | O_APPEND | O_ASYNC | O_DIRECT | O_DSYNC | O_NOATIME | O_NONBLOCK | O_SYNC)


static struct file* filetable = NULL;
static spinlock_t filetable_lock;

static unsigned int lowestfree = 0;



void fd_init(void) {

    filetable  = (struct file*)kcalloc(CONFIG_FILE_MAX, sizeof(struct file), GFP_KERNEL);
    lowestfree = 0;

    spinlock_init(&filetable_lock);
}


/**
 * @brief Allocates an open file for an inode, taking a reference to the inode.
 *
 * @param inode The inode.
 * @param position The initial position.
 * @param flags The open() flags, of which the access mode and status flags are kept.
 * @return The file, holding one reference, or NULL with errno ENFILE.
 */
struct file* fd_append(inode_t* inode, off_t position, int flags) {

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
            filetable[i].flags    = flags & FILE_STATUS_FLAGS;

            spinlock_init(&filetable[i].lock);
            break;
        }
    }


    if (i == CONFIG_FILE_MAX) {
        return errno = ENFILE, NULL;
    }

    vfs_inode_get(inode);

    return &filetable[i];
}


/**
 * @brief Drops a reference to an open file, releasing its slot, closing it and dropping its inode on the last one.
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
            fd->flags    = 0;


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
    vfs_inode_put(inode);
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
 * @param flags Receives the file's access mode and status flags, or NULL.
 * @return The file, or NULL if @p fd is not open.
 */
struct file* fd_get(unsigned int fd, int* flags) {

    DEBUG_ASSERT(current_task);


    if (unlikely(fd >= CONFIG_OPEN_MAX))
        return NULL;


    struct file* file = NULL;

    shared_ptr_access(current_task->fd, fds, {
        if ((file = fds->descriptors[fd].ref) != NULL)
            fd_ref(file);
    });

    if (file && flags)
        *flags = fd_flags(file);

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
 * @brief Reads the access mode and status flags of an open file.
 *
 * @param file The file.
 * @return The flags.
 */
int fd_flags(struct file* file) {

    DEBUG_ASSERT(file);


    int flags = 0;

    scoped_lock(&file->lock) {
        flags = file->flags;
    }

    return flags;
}


/**
 * @brief Changes some of the status flags of an open file, for every descriptor that refers to it.
 *
 * @param file The file.
 * @param mask The flags to change.
 * @param flags Their new values.
 */
void fd_set_flags(struct file* file, int mask, int flags) {

    DEBUG_ASSERT(file);

    scoped_lock(&file->lock) {
        file->flags = (file->flags & ~mask) | (flags & mask);
    }
}


/**
 * @brief Duplicates one of the caller's descriptors into the lowest free slot at or above a minimum.
 *
 * @param fd The descriptor to duplicate.
 * @param min The lowest slot the copy may take.
 * @param cloexec Whether the copy is closed on exec.
 * @return The new descriptor, or -EBADF, -EINVAL or -EMFILE.
 */
long fd_dup(unsigned int fd, unsigned int min, bool cloexec) {

    DEBUG_ASSERT(current_task);


    if (unlikely(fd >= CONFIG_OPEN_MAX))
        return -EBADF;

    if (unlikely(min >= CONFIG_OPEN_MAX))
        return -EINVAL;


    long e = -EMFILE;

    shared_ptr_access(current_task->fd, fds, {
        if (unlikely(!fds->descriptors[fd].ref))
            return -EBADF;

        scoped_lock(&current_task->lock) {

            for (unsigned int i = min; i < CONFIG_OPEN_MAX; i++) {

                if (fds->descriptors[i].ref)
                    continue;

                fds->descriptors[i].ref           = fds->descriptors[fd].ref;
                fds->descriptors[i].close_on_exec = cloexec;

                fd_ref(fds->descriptors[i].ref);

                e = i;
                break;
            }
        }
    });

    return e;
}


/**
 * @brief Duplicates one of the caller's descriptors into a given slot, closing whatever the slot held.
 *
 * @param fd The descriptor to duplicate.
 * @param newfd The slot to put the copy in, which must differ from @p fd.
 * @param cloexec Whether the copy is closed on exec.
 * @return @p newfd, or -EBADF.
 */
long fd_dup_to(unsigned int fd, unsigned int newfd, bool cloexec) {

    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(fd != newfd);


    if (unlikely(fd >= CONFIG_OPEN_MAX || newfd >= CONFIG_OPEN_MAX))
        return -EBADF;


    struct file* old = NULL;

    shared_ptr_access(current_task->fd, fds, {
        if (unlikely(!fds->descriptors[fd].ref))
            return -EBADF;

        scoped_lock(&current_task->lock) {

            old = fds->descriptors[newfd].ref;

            fds->descriptors[newfd].ref           = fds->descriptors[fd].ref;
            fds->descriptors[newfd].close_on_exec = cloexec;

            fd_ref(fds->descriptors[newfd].ref);
        }
    });


    if (old)
        fd_remove(old, true);

    return newfd;
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

        fds->descriptors[i].ref           = NULL;
        fds->descriptors[i].close_on_exec = false;
    }
}


/**
 * @brief Takes a reference to every open file in a table that has just been copied.
 *
 * @param fds The copy.
 */
void fd_ref_all(struct fd* fds) {

    DEBUG_ASSERT(fds);

    for (size_t i = 0; i < CONFIG_OPEN_MAX; i++) {

        if (fds->descriptors[i].ref)
            fd_ref(fds->descriptors[i].ref);
    }
}


/**
 * @brief Makes one attempt at reading from an open file at its position, advancing it, without waiting.
 *
 * @param file The file.
 * @param buf The buffer, in memory the caller has made accessible with uio_lock().
 * @param size The number of bytes to read.
 * @return The number of bytes read, or a negative errno, -EAGAIN when nothing is ready yet.
 */
ssize_t fd_read(struct file* file, void* buf, size_t size) {

    DEBUG_ASSERT(file);
    DEBUG_ASSERT(file->inode);


    ssize_t e = 0;

    scoped_lock(&file->lock) {
        if ((e = vfs_read(file->inode, buf, file->position, size)) > 0)
            file->position += e;
    }

    return e;
}


/**
 * @brief Makes one attempt at writing to an open file at its position, advancing it, without waiting.
 *
 * @param file The file.
 * @param buf The data, in memory the caller has made accessible with uio_lock().
 * @param size The number of bytes to write.
 * @return The number of bytes written, or a negative errno, -EAGAIN when there is no room yet.
 */
ssize_t fd_write(struct file* file, const void* buf, size_t size) {

    DEBUG_ASSERT(file);
    DEBUG_ASSERT(file->inode);


    ssize_t e = 0;

    scoped_lock(&file->lock) {
        if ((e = vfs_write(file->inode, buf, file->position, size)) > 0)
            file->position += e;
    }

    return e;
}
