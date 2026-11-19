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
#include <poll.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>


#define EVENTFD_FSID      0xDEADCB00
#define EVENTFD_FIRST_INO 0xFFFFFFFFE100000
#define EVENTFD_MAX       (UINT64_MAX - 1)
#define EVENTFD_SEMAPHORE 1


struct eventfd {
    uint64_t count;
    bool semaphore;
};


static struct superblock eventfd_superblock = {
    .fsid   = EVENTFD_FSID,
    .flags  = ST_NOATIME | ST_NODEV | ST_NOSUID | ST_NOEXEC,
    .dev    = NULL,
    .root   = NULL,
    .ino    = EVENTFD_FIRST_INO,
    .fsinfo = NULL,
    .st     = {.f_bsize = PML1_PAGESIZE, .f_frsize = PML1_PAGESIZE, .f_fsid = EVENTFD_FSID, .f_namemax = CONFIG_MAXNAMLEN},
};

static _Atomic ino64_t __eventfd_next_ino = EVENTFD_FIRST_INO + 1;


static ssize_t eventfd_read(inode_t* inode, void* buf, off_t pos, size_t len) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    __unused_param(pos);

    struct eventfd* efd = (struct eventfd*)inode->userdata;

    if (unlikely(len < sizeof(uint64_t))) {
        return -EINVAL;
    }

    if (efd->count == 0) {
        return -EAGAIN;
    }

    uint64_t value = efd->semaphore ? 1 : efd->count;

    efd->count -= value;

    memcpy(buf, &value, sizeof(value));

    return sizeof(uint64_t);
}


static ssize_t eventfd_write(inode_t* inode, const void* buf, off_t pos, size_t len) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    __unused_param(pos);

    struct eventfd* efd = (struct eventfd*)inode->userdata;

    if (unlikely(len < sizeof(uint64_t))) {
        return -EINVAL;
    }

    uint64_t value = 0;
    memcpy(&value, buf, sizeof(value));

    if (unlikely(value == UINT64_MAX)) {
        return -EINVAL;
    }

    if (EVENTFD_MAX - efd->count < value) {
        return -EAGAIN;
    }

    efd->count += value;

    return sizeof(uint64_t);
}


static int eventfd_poll(inode_t* inode, int events) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    struct eventfd* efd = (struct eventfd*)inode->userdata;

    int revents = 0;

    if (efd->count > 0) {
        revents |= POLLIN | POLLRDNORM;
    }

    if (efd->count < EVENTFD_MAX) {
        revents |= POLLOUT | POLLWRNORM;
    }

    return revents & events;
}


static int eventfd_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(st);

    memset(st, 0, sizeof(*st));

    st->st_ino     = inode->ino;
    st->st_mode    = 0600;
    st->st_nlink   = 1;
    st->st_blksize = PML1_PAGESIZE;

    st->st_mtime = arch_timer_gettime();
    st->st_atime = arch_timer_gettime();
    st->st_ctime = arch_timer_gettime();

    return 0;
}


static void eventfd_release(inode_t* inode) {

    DEBUG_ASSERT(inode);

    if (inode->userdata) {
        kfree(inode->userdata);
    }

    inode->userdata = NULL;
}


inode_t* eventfd_inode(uint64_t count, bool semaphore) {

    inode_t* inode = (inode_t*)kcalloc(1, sizeof(inode_t), GFP_KERNEL);

    if (unlikely(!inode)) {
        return errno = ENOMEM, NULL;
    }

    struct eventfd* efd = (struct eventfd*)kcalloc(1, sizeof(struct eventfd), GFP_KERNEL);

    if (unlikely(!efd)) {
        kfree(inode);
        return errno = ENOMEM, NULL;
    }

    inode->ev = shared_ptr_new(struct inode_events, GFP_KERNEL);

    if (unlikely(!inode->ev)) {
        kfree(efd);
        kfree(inode);
        return errno = ENOMEM, NULL;
    }


    efd->count     = count;
    efd->semaphore = semaphore;

    strcpy(inode->name, "anon_inode:[eventfd]");

    inode->ino      = atomic_fetch_add(&__eventfd_next_ino, 1);
    inode->sb       = &eventfd_superblock;
    inode->parent   = NULL;
    inode->flags    = INODE_FLAGS_ANONYMOUS;
    inode->userdata = efd;

    inode->ops.read    = eventfd_read;
    inode->ops.write   = eventfd_write;
    inode->ops.poll    = eventfd_poll;
    inode->ops.getattr = eventfd_getattr;
    inode->ops.release = eventfd_release;

    spinlock_init(&inode->lock);

    return inode;
}


long eventfd_create_fd(unsigned int count, int flags) {

    if (unlikely(flags & ~(EVENTFD_SEMAPHORE | O_CLOEXEC | O_NONBLOCK))) {
        return -EINVAL;
    }

    inode_t* inode = eventfd_inode(count, !!(flags & EVENTFD_SEMAPHORE));

    if (unlikely(!inode)) {
        return -ENOMEM;
    }

    struct file* ref = fd_append(inode, 0, O_RDWR | (flags & O_NONBLOCK));

    if (unlikely(!ref)) {
        vfs_anonymous_free(inode);
        return -ENFILE;
    }

    long fd = fd_install(ref, !!(flags & O_CLOEXEC));

    if (unlikely(fd < 0)) {
        fd_remove(ref, true);
    }

    return fd;
}
