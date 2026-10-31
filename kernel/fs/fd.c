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
#include <sys/stat.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/memory.h>
#include <aplus/task.h>
#include <aplus/vfs.h>


#define FILE_STATUS_FLAGS (O_ACCMODE | O_APPEND | O_ASYNC | O_DIRECT | O_DSYNC | O_NOATIME | O_NONBLOCK | O_SYNC)


static struct file* filetable = NULL;
static spinlock_t filetable_lock;

static unsigned int lowestfree = 0;



void fd_init(void) {

    filetable  = (struct file*)kcalloc(CONFIG_FILE_MAX, sizeof(struct file), GFP_KERNEL);
    lowestfree = 0;

    spinlock_init(&filetable_lock);
}


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


    if (!inode)
        return;

    vfs_lock_release(inode, (uintptr_t)fd);

    if (!close)
        return;

    vfs_close(inode);
    vfs_inode_put(inode);
}


void fd_ref(struct file* file) {

    DEBUG_ASSERT(file);
    DEBUG_ASSERT(filetable);
    DEBUG_ASSERT(atomic_load(&file->refcount) > 0);

    atomic_fetch_add(&file->refcount, 1);
}


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


void fd_put(struct file* file) {

    fd_remove(file, true);
}


int fd_flags(struct file* file) {

    DEBUG_ASSERT(file);


    int flags = 0;

    scoped_lock(&file->lock) {
        flags = file->flags;
    }

    return flags;
}


void fd_set_flags(struct file* file, int mask, int flags) {

    DEBUG_ASSERT(file);

    scoped_lock(&file->lock) {
        file->flags = (file->flags & ~mask) | (flags & mask);
    }
}


unsigned int fd_limit(void) {

    DEBUG_ASSERT(current_task);

    rlim_t limit = current_task->rlimits[RLIMIT_NOFILE].rlim_cur;

    return limit < CONFIG_OPEN_MAX ? (unsigned int)limit : CONFIG_OPEN_MAX;
}


long fd_dup(unsigned int fd, unsigned int min, bool cloexec) {

    DEBUG_ASSERT(current_task);


    if (unlikely(fd >= CONFIG_OPEN_MAX))
        return -EBADF;

    const unsigned int limit = fd_limit();

    if (unlikely(min >= limit))
        return -EINVAL;


    long e = -EMFILE;

    shared_ptr_access(current_task->fd, fds, {
        if (unlikely(!fds->descriptors[fd].ref))
            return -EBADF;

        scoped_lock(&current_task->lock) {

            for (unsigned int i = min; i < limit; i++) {

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


long fd_dup_to(unsigned int fd, unsigned int newfd, bool cloexec) {

    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(fd != newfd);


    if (unlikely(fd >= CONFIG_OPEN_MAX || newfd >= fd_limit()))
        return -EBADF;


    struct file* old = NULL;
    uintptr_t owner  = 0;

    shared_ptr_access(current_task->fd, fds, {
        if (unlikely(!fds->descriptors[fd].ref))
            return -EBADF;

        owner = (uintptr_t)fds;

        scoped_lock(&current_task->lock) {

            old = fds->descriptors[newfd].ref;

            fds->descriptors[newfd].ref           = fds->descriptors[fd].ref;
            fds->descriptors[newfd].close_on_exec = cloexec;

            fd_ref(fds->descriptors[newfd].ref);
        }
    });


    if (old) {

        if (old->inode)
            vfs_lock_release(old->inode, owner);

        fd_remove(old, true);
    }

    return newfd;
}


long fd_sync(unsigned int fd, int datasync) {

    int flags         = 0;
    struct file* file = fd_get(fd, &flags);

    if (unlikely(!file))
        return -EBADF;


    long e = 0;

    if (unlikely(flags & O_PATH)) {

        e = -EBADF;

    } else if (vfs_fsync(file->inode, datasync) < 0) {

        e = -errno;

        if (e == -ENOSYS) {

            struct stat st;

            if (vfs_getattr(file->inode, &st) == 0 && (S_ISREG(st.st_mode) || S_ISDIR(st.st_mode)))
                e = 0;
            else
                e = -EINVAL;
        }
    }

    fd_put(file);

    return e;
}


void fd_close_all(struct fd* fds) {

    DEBUG_ASSERT(fds);

    for (size_t i = 0; i < CONFIG_OPEN_MAX; i++) {

        if (!fds->descriptors[i].ref)
            continue;

        if (fds->descriptors[i].ref->inode)
            vfs_lock_release(fds->descriptors[i].ref->inode, (uintptr_t)fds);

        fd_remove(fds->descriptors[i].ref, true);

        fds->descriptors[i].ref           = NULL;
        fds->descriptors[i].close_on_exec = false;
    }
}


void fd_ref_all(struct fd* fds) {

    DEBUG_ASSERT(fds);

    for (size_t i = 0; i < CONFIG_OPEN_MAX; i++) {

        if (fds->descriptors[i].ref)
            fd_ref(fds->descriptors[i].ref);
    }
}


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


ssize_t fd_write(struct file* file, const void* buf, size_t size) {

    DEBUG_ASSERT(file);
    DEBUG_ASSERT(file->inode);


    ssize_t e = 0;

    scoped_lock(&file->lock) {

        off_t off = file->position;

        if (file->flags & O_APPEND)
            e = vfs_write_append(file->inode, buf, &off, size);
        else
            e = vfs_write(file->inode, buf, off, size);

        if (e > 0)
            file->position = off + e;
    }

    return e;
}


ssize_t fd_writev(struct file* file, const struct iovec* iov, size_t count) {

    DEBUG_ASSERT(file);
    DEBUG_ASSERT(file->inode);


    ssize_t e = 0;

    scoped_lock(&file->lock) {

        off_t off = file->position;

        if ((e = vfs_writev(file->inode, iov, count, &off, !!(file->flags & O_APPEND))) > 0)
            file->position = off + e;
    }

    return e;
}
