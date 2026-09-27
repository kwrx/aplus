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

/**
 * @brief lwIP sockets as ordinary file descriptors: an anonymous inode carrying the lwIP index.
 *
 * dup(), fork() inheritance, close-on-exec, poll() and last-close teardown then need no socket-specific code.
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


#include <aplus/network.h>


/**
 * @brief Distinct from the local-socket filesystem in kernel/ipc/unix.c, which the superblock tells apart.
 */
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

static _Atomic ino64_t __sockfs_next_ino = SOCKFS_FIRST_INO + 1;


/**
 * @brief The lwIP index lives in inode->userdata, biased by one so that index 0 stays distinct from NULL.
 */
#define SOCKFS_ENCODE(s) ((void*)(uintptr_t)((s) + 1))
#define SOCKFS_DECODE(p) ((int)(uintptr_t)(p) - 1)


int socket_poll_arm(inode_t* inode, int events, struct timespec* timeout);


/**
 * @brief Reports the lwIP socket an inode stands for.
 *
 * @param inode The inode to ask about.
 * @return The lwIP socket, or -1 if the inode does not stand for one.
 */
int socket_from_inode(inode_t* inode) {

    if (unlikely(!inode))
        return -1;

    if (inode->sb != &sockfs_superblock)
        return -1;

    return SOCKFS_DECODE(inode->userdata);
}


/**
 * @brief Reports the lwIP socket a descriptor refers to.
 *
 * @param fd The descriptor to ask about.
 * @return The lwIP socket, or -1 if the descriptor is not one.
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

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            socket_poll_arm(inode, POLLIN, NULL);

        return -errno;
    }

    return e;
}


/**
 * @brief The largest piece of a stream write copied into the kernel at once, which keeps the copy within eight pages.
 */
#define SOCKFS_CHUNK_MAX ((8 * PML1_PAGESIZE) - 64)


/**
 * @brief Tells whether an lwIP socket is a byte stream, whose writes may be split into pieces.
 *
 * @param socket The lwIP socket.
 * @return true for SOCK_STREAM.
 */
static bool __socket_is_stream(int socket) {

    int type      = 0;
    socklen_t len = sizeof(type);

    return lwip_getsockopt(socket, SOL_SOCKET, SO_TYPE, &type, &len) == 0 && type == SOCK_STREAM;
}


/**
 * @brief Sends caller data on an lwIP socket through a kernel copy of it.
 *
 * A blocking TCP write is finished by the lwIP thread, which runs in the kernel address space and cannot read
 * the caller's pages. A stream is copied a piece at a time; a datagram is copied whole, since splitting it
 * would change what the peer receives.
 *
 * @param socket The lwIP socket.
 * @param buf The data, in memory the caller has made accessible with uio_lock().
 * @param size How many bytes to send.
 * @param flags MSG_* flags.
 * @param to The destination, already in kernel memory, or NULL.
 * @param tolen The length of @p to.
 * @return The number of bytes sent, or -1 with errno set.
 */
ssize_t socket_send(int socket, const void* buf, size_t size, int flags, const struct sockaddr* to, socklen_t tolen) {

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    size_t chunk = size;

    if (size > SOCKFS_CHUNK_MAX && __socket_is_stream(socket))
        chunk = SOCKFS_CHUNK_MAX;

    void* kbuf = kmalloc(chunk, GFP_KERNEL);

    if (unlikely(!kbuf))
        return errno = ENOMEM, -1;


    size_t sent = 0;
    ssize_t e   = 0;

    while (sent < size) {

        size_t n = size - sent < chunk ? size - sent : chunk;

        memcpy(kbuf, (const uint8_t*)buf + sent, n);

        if (to)
            e = lwip_sendto(socket, kbuf, n, flags, to, tolen);
        else
            e = lwip_send(socket, kbuf, n, flags);

        if (e <= 0)
            break;

        sent += (size_t)e;

        if ((size_t)e < n)
            break;
    }

    kfree(kbuf);

    return sent > 0 ? (ssize_t)sent : e;
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


    ssize_t e = socket_send(socket, buf, size, 0, NULL, 0);

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


/**
 * @brief Reports what an lwIP socket is ready for, without sleeping.
 *
 * @param inode The socket inode.
 * @param events Mask of the events the caller cares about.
 * @return What is actually ready.
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
 * @brief Parks the caller on an lwIP socket until something about it moves.
 *
 * Registers only; the caller suspends once after arming everything it watches.
 *
 * @param inode The socket inode.
 * @param events Mask of the events the caller cares about.
 * @param timeout Time left to sleep, or NULL to wait indefinitely.
 * @return 1 if @p inode is a socket and was armed, 0 if it is not a socket, or a negative error number.
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
    inode->ino     = atomic_fetch_add(&__sockfs_next_ino, 1);
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

    inode->ev = NULL;

    spinlock_init(&inode->lock);

    return inode;
}


/**
 * @brief Installs an lwIP socket into the caller's descriptor table, taking ownership of it.
 *
 * @param socket Index returned by lwip_socket() or lwip_accept().
 * @param flags O_NONBLOCK and O_CLOEXEC as requested by socket()/accept4().
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
        vfs_anonymous_free(inode);

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

        fd_remove(ref, true);
    }

    return fd;
}
