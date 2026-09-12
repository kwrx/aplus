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

/*
 * lwIP sockets as ordinary file descriptors.
 *
 * These used to live in a numbering space of their own -- the lwIP index plus CONFIG_OPEN_MAX
 * -- which meant a socket was not a descriptor at all. Every syscall that took an fd had to
 * either special-case the range or reject it, and the ones that reject it are not obscure:
 * dup(), dup2(), fstat() and mmap() all answered EBADF for a perfectly good socket. That is
 * enough to break any program that moves an accepted connection onto stdin/stdout, which is
 * what inetd-style servers and CGI do as a matter of course.
 *
 * So a socket is given the same shape the local ones already had (see kernel/ipc/unix.c): an
 * anonymous inode carrying the lwIP index, wrapped in a struct file, installed in the caller's
 * descriptor table like anything else. dup(), fork() inheritance, close-on-exec, poll() and
 * last-close teardown then work through the paths that already exist, with no socket-specific
 * code in any of them.
 */

#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/vfs.h>

#if defined(CONFIG_HAVE_NETWORK)

    #include <aplus/network.h>


    //? Distinct from the local-socket filesystem in kernel/ipc/unix.c: the two are told apart
    //? by their superblock, so they must not share one.
    #define SOCKFS_FSID      0xDEADB0CF
    #define SOCKFS_FIRST_INO 0xFFFFFFFFF000000


static struct superblock sockfs_superblock = {
    .fsid   = SOCKFS_FSID,
    .flags  = ST_NOATIME | ST_NODEV | ST_NOSUID | ST_NOEXEC | ST_SYNCHRONOUS,
    .dev    = NULL,
    .root   = NULL,
    .ino    = SOCKFS_FIRST_INO,
    .fsinfo = NULL,
    .st     = {.f_bsize = CONFIG_BUFSIZ, .f_frsize = CONFIG_BUFSIZ, .f_fsid = SOCKFS_FSID, .f_namemax = 0},
};

static ino64_t __sockfs_next_ino = SOCKFS_FIRST_INO + 1;


//? The lwIP index lives in inode->userdata, biased by one so that index 0 stays distinct from
//? the NULL that a torn-down endpoint leaves behind.
    #define SOCKFS_ENCODE(s) ((void*)(uintptr_t)((s) + 1))
    #define SOCKFS_DECODE(p) ((int)(uintptr_t)(p) - 1)


int socket_poll_arm(inode_t* inode, int events, struct timespec* timeout);


/**
 * @brief The lwIP socket an inode stands for, or -1 if it does not stand for one.
 */
int socket_from_inode(inode_t* inode) {

    if (unlikely(!inode))
        return -1;

    //? Checked by superblock rather than by ops: a local socket has the same ops layout and
    //? would otherwise be mistaken for an lwIP index.
    if (inode->sb != &sockfs_superblock)
        return -1;

    return SOCKFS_DECODE(inode->userdata);
}


/**
 * @brief The lwIP socket a descriptor refers to, or -1 if that descriptor is not one.
 */
int socket_from_fd(int fd) {

    if (unlikely(fd < 0 || fd >= CONFIG_OPEN_MAX))
        return -1;


    int socket = -1;

    shared_ptr_access(current_task->fd, fds, {
        if (fds->descriptors[fd].ref != NULL)
            socket = socket_from_inode(fds->descriptors[fd].ref->inode);
    });

    return socket;
}


static ssize_t sockfs_read(inode_t* inode, void* buf, off_t offset, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);

    __unused_param(offset);


    int socket = socket_from_inode(inode);

    if (unlikely(socket < 0))
        return -EIO;

    if (unlikely(size == 0))
        return 0;


    ssize_t e = lwip_read(socket, buf, size);

    if (unlikely(e < 0)) {

        //? A blocking socket sleeps inside lwIP and never lands here, so this is a socket
        //? someone made non-blocking. The caller may still decide to wait -- read() does when
        //? the descriptor itself is not O_NONBLOCK -- and it would wait on this inode's event
        //? counter, which a socket does not have. Arming lwIP's queue first means that if the
        //? caller does suspend, there is something on the other side to wake it.
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            socket_poll_arm(inode, POLLIN, NULL);

        return -errno;
    }

    return e;
}


static ssize_t sockfs_write(inode_t* inode, const void* buf, off_t offset, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);

    __unused_param(offset);


    int socket = socket_from_inode(inode);

    if (unlikely(socket < 0))
        return -EIO;

    if (unlikely(size == 0))
        return 0;


    ssize_t e = lwip_write(socket, buf, size);

    if (unlikely(e < 0)) {

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            socket_poll_arm(inode, POLLOUT, NULL);

        return -errno;
    }

    return e;
}


static int sockfs_ioctl(inode_t* inode, long req, void* arg) {

    DEBUG_ASSERT(inode);


    int socket = socket_from_inode(inode);

    if (unlikely(socket < 0))
        return -EIO;


    int e = lwip_ioctl(socket, req, arg);

    if (unlikely(e < 0))
        return -errno;

    return e;
}


/*
 * Readiness now, without sleeping. The waiting half is socket_poll_arm(), because lwIP has its
 * own wait queue and an inode event counter would only be a second, lagging copy of it.
 */
static int sockfs_poll(inode_t* inode, int events) {

    DEBUG_ASSERT(inode);


    int socket = socket_from_inode(inode);

    if (unlikely(socket < 0))
        return POLLERR;


    struct pollfd sfd = {.fd = socket, .events = (short)events, .revents = 0};

    if (unlikely(lwip_poll_from_syscall(&sfd, 1, NULL, false) < 0))
        return POLLERR;

    return sfd.revents & (events | POLLHUP | POLLERR | POLLNVAL);
}


/**
 * @brief Park the caller on an lwIP socket until something about it moves.
 *
 * Registers only; the caller suspends once after arming everything it watches.
 *
 * @return 1 if @p inode is a socket and was armed, 0 if it is not a socket, or a negative
 *         error number.
 */
int socket_poll_arm(inode_t* inode, int events, struct timespec* timeout) {

    int socket = socket_from_inode(inode);

    if (socket < 0)
        return 0;


    struct pollfd sfd = {.fd = socket, .events = (short)events, .revents = 0};

    if (unlikely(lwip_poll_from_syscall(&sfd, 1, timeout, true) < 0))
        return -errno;

    return 1;
}


static int sockfs_close(inode_t* inode) {

    DEBUG_ASSERT(inode);


    int socket = socket_from_inode(inode);

    if (unlikely(socket < 0))
        return 0;


    //? Cleared first: the inode outlives this call by a moment, and a second close of an index
    //? lwIP has already handed out again would tear down somebody else's connection.
    inode->userdata = NULL;

    if (unlikely(lwip_close(socket) < 0))
        return -errno;

    return 0;
}


static int sockfs_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(st);


    st->st_ino     = inode->ino;
    st->st_mode    = S_IFSOCK | 0666;
    st->st_nlink   = 1;
    st->st_size    = 0;
    st->st_blksize = CONFIG_BUFSIZ;

    st->st_mtime = arch_timer_gettime();
    st->st_atime = arch_timer_gettime();
    st->st_ctime = arch_timer_gettime();

    return 0;
}


static inode_t* __sockfs_inode(int socket) {

    inode_t* inode = kcalloc(1, sizeof(inode_t), GFP_KERNEL);

    if (unlikely(!inode))
        return NULL;

    inode->name[0] = '\0';
    inode->ino     = __sockfs_next_ino++;
    inode->sb      = &sockfs_superblock;
    inode->parent  = NULL;
    inode->flags   = INODE_FLAGS_ANONYMOUS;

    inode->userdata = SOCKFS_ENCODE(socket);

    inode->ops.close   = sockfs_close;
    inode->ops.read    = sockfs_read;
    inode->ops.write   = sockfs_write;
    inode->ops.ioctl   = sockfs_ioctl;
    inode->ops.poll    = sockfs_poll;
    inode->ops.getattr = sockfs_getattr;

    //? No event counter: readiness is lwIP's to report, and waiters are parked on its queue by
    //? socket_poll_arm() rather than on an inode futex. Every reader of inode->ev copes with a
    //? null one.
    inode->ev = NULL;

    spinlock_init(&inode->lock);

    return inode;
}


/**
 * @brief Install an lwIP socket into the caller's descriptor table.
 *
 * Takes ownership of @p socket: on failure it is closed rather than leaked, so a caller that
 * has just created one only has to hand it over and check the result.
 *
 * @param socket    Index returned by lwip_socket() or lwip_accept().
 * @param flags     O_NONBLOCK and O_CLOEXEC as requested by socket()/accept4().
 *
 * @return The new descriptor, or a negative error number.
 */
int socket_install(int socket, int flags) {

    DEBUG_ASSERT(socket >= 0);


    inode_t* inode = __sockfs_inode(socket);

    if (unlikely(!inode)) {

        lwip_close(socket);
        return -ENOMEM;
    }


    struct file* ref = fd_append(inode, 0, 0);

    if (unlikely(!ref)) {

        inode->userdata = NULL;
        kfree(inode);

        lwip_close(socket);
        return -ENFILE;
    }


    int fd = -EMFILE;

    shared_ptr_access(current_task->fd, fds, {
        scoped_lock(&current_task->lock) {
            for (int i = 0; i < CONFIG_OPEN_MAX; i++) {

                if (fds->descriptors[i].ref)
                    continue;

                fds->descriptors[i].ref           = ref;
                fds->descriptors[i].flags         = O_RDWR | (flags & O_NONBLOCK);
                fds->descriptors[i].close_on_exec = !!(flags & O_CLOEXEC);

                fd = i;
                break;
            }
        }
    });


    if (unlikely(fd < 0)) {

        //? fd_remove() runs the inode through vfs_close(), which is what closes the lwIP
        //? socket, and then frees the anonymous inode.
        fd_remove(ref, true);
    }

    return fd;
}

#endif
