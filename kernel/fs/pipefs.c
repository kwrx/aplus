/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>

#include <aplus/utils/ringbuffer.h>


/**
 * @brief A pipe is one buffer shared by two independently closeable endpoints, each with its own inode.
 *
 * The buffer outlives whichever end closes first and is torn down once both are gone.
 */


#define PIPEFS_FSID      0xDEADCAFE
#define PIPEFS_FIRST_INO 0xFFFFFFFFF000000


static struct superblock pipefs_superblock = {
    .fsid   = PIPEFS_FSID,
    .flags  = ST_NOATIME | ST_NODEV | ST_NOSUID | ST_NOEXEC | ST_SYNCHRONOUS,
    .dev    = NULL,
    .root   = NULL,
    .ino    = PIPEFS_FIRST_INO,
    .fsinfo = NULL,
    .st     = {.f_bsize = CONFIG_BUFSIZ, .f_frsize = CONFIG_BUFSIZ, .f_blocks = 0, .f_bfree = 0, .f_bavail = 0, .f_files = 0, .f_ffree = 0, .f_favail = 0, .f_fsid = PIPEFS_FSID, .f_flag = 0, .f_namemax = 0},
};

static ino64_t __pipefs_next_ino = PIPEFS_FIRST_INO + 1;


struct pipe {

    ringbuffer_t rb;

    atomic_int readers;
    atomic_int writers;

    //? One change counter for the whole channel, shared by every endpoint
    //? inode on it. Waking through the channel rather than through a
    //? particular peer inode is what lets a named FIFO have more than one
    //? reader or writer without anyone being left asleep.
    inode_events_t ev;

    //? The directory entry a named FIFO was opened through, so the last close
    //? can detach the channel from it. NULL for an anonymous pipe.
    inode_t* node;

    spinlock_t lock;
};

struct pipe_endpoint {

    struct pipe* pipe;
    int dir;
};


static void __pipe_wake(struct pipe* pipe) {

    if (unlikely(!pipe))
        return;

    shared_ptr_nullable_access(pipe->ev, ev, {
        atomic_fetch_add(&ev->futex, 1);
    });
}


static inline bool __pipe_readable(struct pipe_endpoint* ep) {
    return ep->dir != PIPE_END_WRITE;
}

static inline bool __pipe_writable(struct pipe_endpoint* ep) {
    return ep->dir != PIPE_END_READ;
}


int pipefs_close(inode_t* inode) {

    DEBUG_ASSERT(inode);

    struct pipe_endpoint* ep = (struct pipe_endpoint*)inode->userdata;

    if (unlikely(!ep))
        return 0;

    struct pipe* pipe = ep->pipe;

    DEBUG_ASSERT(pipe);


    bool last = false;

    scoped_lock(&pipe->lock) {

        if (__pipe_readable(ep))
            atomic_fetch_sub(&pipe->readers, 1);

        if (__pipe_writable(ep))
            atomic_fetch_sub(&pipe->writers, 1);

        last = (atomic_load(&pipe->readers) <= 0 && atomic_load(&pipe->writers) <= 0);
    }


    inode->userdata = NULL;
    kfree(ep);

    __pipe_wake(pipe);


    if (last) {

        if (pipe->node) {

            pipe->node->userdata = NULL;
            pipe->node           = NULL;
        }

        ringbuffer_destroy(&pipe->rb);

        kfree(pipe);
    }

    return 0;
}


ssize_t pipefs_read(inode_t* inode, void* buf, off_t offset, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);

    __unused_param(offset);


    struct pipe_endpoint* ep = (struct pipe_endpoint*)inode->userdata;

    if (unlikely(!ep))
        return -EIO;

    if (unlikely(!__pipe_readable(ep)))
        return -EBADF;

    if (unlikely(size == 0))
        return 0;


    ssize_t e = ringbuffer_read(&ep->pipe->rb, buf, size);

    if (e == -EAGAIN && atomic_load(&ep->pipe->writers) <= 0)
        return 0;

    if (e > 0)
        __pipe_wake(ep->pipe);

    return e;
}


ssize_t pipefs_write(inode_t* inode, const void* buf, off_t offset, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);

    __unused_param(offset);


    struct pipe_endpoint* ep = (struct pipe_endpoint*)inode->userdata;

    if (unlikely(!ep))
        return -EIO;

    if (unlikely(!__pipe_writable(ep)))
        return -EBADF;

    if (unlikely(size == 0))
        return 0;


    if (unlikely(atomic_load(&ep->pipe->readers) <= 0))
        return -EPIPE;


    ssize_t e = ringbuffer_write(&ep->pipe->rb, buf, size);

    if (e > 0)
        __pipe_wake(ep->pipe);

    return e;
}


int pipefs_poll(inode_t* inode, int events) {

    DEBUG_ASSERT(inode);

    struct pipe_endpoint* ep = (struct pipe_endpoint*)inode->userdata;

    if (unlikely(!ep))
        return POLLERR;


    struct pipe* pipe = ep->pipe;

    int revents = 0;


    if (__pipe_readable(ep)) {

        if (ringbuffer_available(&pipe->rb) > 0)
            revents |= POLLIN;

        if (atomic_load(&pipe->writers) <= 0)
            revents |= POLLHUP;
    }


    if (__pipe_writable(ep)) {

        if (ringbuffer_writeable(&pipe->rb) > 0)
            revents |= POLLOUT;

        if (atomic_load(&pipe->readers) <= 0)
            revents |= POLLERR;
    }


    return revents & (events | POLLHUP | POLLERR);
}


int pipefs_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(st);

    struct pipe_endpoint* ep = (struct pipe_endpoint*)inode->userdata;

    st->st_ino     = inode->ino;
    st->st_mode    = S_IFIFO | 0666;
    st->st_rdev    = 0;
    st->st_nlink   = 1;
    st->st_size    = ep ? (off_t)ringbuffer_available(&ep->pipe->rb) : 0;
    st->st_blksize = CONFIG_BUFSIZ;
    st->st_blocks  = (st->st_size + CONFIG_BUFSIZ - 1) / CONFIG_BUFSIZ;

    st->st_mtime = arch_timer_gettime();
    st->st_atime = arch_timer_gettime();
    st->st_ctime = arch_timer_gettime();

    return 0;
}


inode_t* pipefs_inode(void) {

    inode_t* inode = kcalloc(1, sizeof(inode_t), GFP_KERNEL);

    if (unlikely(!inode))
        return errno = ENOMEM, NULL;


    inode->name[0] = '\0';
    inode->ino     = __pipefs_next_ino++;
    inode->sb      = &pipefs_superblock;
    inode->parent  = NULL;

    inode->flags = INODE_FLAGS_ANONYMOUS;

    spinlock_init(&inode->lock);

    return inode;
}


static void __pipefs_attach(inode_t* inode, struct pipe* pipe, int dir) {

    struct pipe_endpoint* ep = kcalloc(1, sizeof(struct pipe_endpoint), GFP_KERNEL);

    DEBUG_ASSERT(ep);

    ep->pipe = pipe;
    ep->dir  = dir;

    inode->userdata    = ep;
    inode->ops.open    = NULL;
    inode->ops.close   = pipefs_close;
    inode->ops.read    = pipefs_read;
    inode->ops.write   = pipefs_write;
    inode->ops.getattr = pipefs_getattr;
    inode->ops.poll    = pipefs_poll;

    inode->ev = shared_ptr_ref(pipe->ev);
}


static struct pipe* __pipefs_channel(size_t bufsize) {

    struct pipe* pipe = kcalloc(1, sizeof(struct pipe), GFP_KERNEL);

    if (unlikely(!pipe))
        return NULL;

    if (unlikely(ringbuffer_init(&pipe->rb, bufsize) < 0)) {

        kfree(pipe);
        return NULL;
    }

    pipe->ev = shared_ptr_new(struct inode_events, GFP_KERNEL);

    if (unlikely(!pipe->ev)) {

        ringbuffer_destroy(&pipe->rb);
        kfree(pipe);

        return NULL;
    }

    spinlock_init(&pipe->lock);

    return pipe;
}


int pipefs_create_pair(inode_t** rd, inode_t** wr, size_t bufsize) {

    DEBUG_ASSERT(rd);
    DEBUG_ASSERT(wr);
    DEBUG_ASSERT(bufsize);


    *rd = NULL;
    *wr = NULL;


    struct pipe* pipe = __pipefs_channel(bufsize);

    if (unlikely(!pipe))
        return -ENOMEM;


    inode_t* r = pipefs_inode();
    inode_t* w = pipefs_inode();

    if (unlikely(!r || !w)) {

        if (r)
            kfree(r);

        if (w)
            kfree(w);

        ringbuffer_destroy(&pipe->rb);
        kfree(pipe);

        return -ENOMEM;
    }


    atomic_store(&pipe->readers, 1);
    atomic_store(&pipe->writers, 1);

    __pipefs_attach(r, pipe, PIPE_END_READ);
    __pipefs_attach(w, pipe, PIPE_END_WRITE);


    *rd = r;
    *wr = w;

    return 0;
}


/**
 * @brief Hands out a fresh endpoint on a named FIFO, creating the channel behind it on the first open.
 *
 * @param node The FIFO directory entry.
 * @param flags The open flags, whose access mode selects the endpoint direction.
 * @return The endpoint inode, or NULL with errno set.
 */

inode_t* fifofs_open(inode_t* node, int flags) {

    DEBUG_ASSERT(node);

    struct pipe* pipe = (struct pipe*)node->userdata;

    if (!pipe) {

        if (unlikely(!(pipe = __pipefs_channel(CONFIG_PIPESIZ))))
            return errno = ENOMEM, NULL;

        pipe->node     = node;
        node->userdata = pipe;
    }


    int dir;

    switch (flags & O_ACCMODE) {

        case O_RDONLY:
            dir = PIPE_END_READ;
            break;

        case O_WRONLY:
            dir = PIPE_END_WRITE;
            break;

        default:
            dir = PIPE_END_BOTH;
            break;
    }


    inode_t* end = pipefs_inode();

    if (unlikely(!end))
        return errno = ENOMEM, NULL;


    scoped_lock(&pipe->lock) {

        if (dir != PIPE_END_WRITE)
            atomic_fetch_add(&pipe->readers, 1);

        if (dir != PIPE_END_READ)
            atomic_fetch_add(&pipe->writers, 1);
    }

    __pipefs_attach(end, pipe, dir);

    strncpy(end->name, node->name, CONFIG_MAXNAMLEN - 1);

    return end;
}


/**
 * @brief Turns a freshly created directory entry into a FIFO node, installing only its open handler.
 *
 * @param inode The directory entry to convert.
 * @param bufsize Unused.
 * @param flags Unused.
 * @return The inode passed in.
 */

inode_t* vfs_mkfifo(inode_t* inode, size_t bufsize, int flags) {

    DEBUG_ASSERT(inode);

    __unused_param(bufsize);
    __unused_param(flags);

    inode->ops.open = fifofs_open;

    return inode;
}
