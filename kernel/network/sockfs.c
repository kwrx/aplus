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
#include <sys/time.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/poll.h>
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


/**
 * @brief The send()/recv() flags as Linux numbers them, which differ from lwIP's.
 */
#define LINUX_MSG_OOB      0x0001
#define LINUX_MSG_PEEK     0x0002
#define LINUX_MSG_DONTWAIT 0x0040
#define LINUX_MSG_WAITALL  0x0100
#define LINUX_MSG_MORE     0x8000


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

    if (unlikely(e < 0))
        return -errno;

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
 * @brief Sends caller data on an lwIP socket through a kernel copy of it, without waiting.
 *
 * A stream is copied a piece at a time; a datagram is copied whole, since splitting it would change what the peer
 * receives.
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

    if (unlikely(e < 0))
        return -errno;

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

    if (unlikely(lwip_poll_from_syscall(&sfd, 1) < 0))
        return POLLERR;

    return sfd.revents & (events | POLLHUP | POLLERR | POLLNVAL);
}


/**
 * @brief Finds the futex word an lwIP socket bumps whenever its readiness may have changed.
 *
 * @param inode The inode to ask about.
 * @return The word, or NULL if @p inode is not an lwIP socket.
 */
volatile uint32_t* socket_event_word(inode_t* inode) {

    int socket = socket_from_inode(inode);

    if (socket < 0)
        return NULL;

    return (volatile uint32_t*)lwip_socket_event(socket);
}


/**
 * @brief Registers the caller to be woken when an lwIP socket moves.
 *
 * Registers only; the caller rescans after arming everything it watches, then suspends once.
 *
 * @param inode The socket inode.
 * @param events Mask of the events the caller cares about.
 * @param timeout Time left to sleep, or NULL to wait indefinitely.
 * @return 1 if @p inode is a socket and was armed, 0 if it is not a socket.
 */
int socket_poll_arm(inode_t* inode, int events, struct timespec* timeout) {

    __unused_param(events);


    volatile uint32_t* word = socket_event_word(inode);

    if (!word)
        return 0;

    futex_wait(current_task, word, *word, timeout);

    return 1;
}


/**
 * @brief Reports how long a wait on a socket may last, from SO_RCVTIMEO or SO_SNDTIMEO.
 *
 * @param inode The socket inode.
 * @param send Whether the wait is for room to send rather than for data to receive.
 * @return The timeout in nanoseconds, or POLL_TIMEOUT_FOREVER if none is set or @p inode is not an lwIP socket.
 */
uint64_t socket_timeout(inode_t* inode, bool send) {

    int socket = socket_from_inode(inode);

    if (socket < 0)
        return POLL_TIMEOUT_FOREVER;


    struct timeval tv = {0};
    socklen_t len     = sizeof(tv);

    if (lwip_getsockopt(socket, SOL_SOCKET, send ? SO_SNDTIMEO : SO_RCVTIMEO, &tv, &len) < 0)
        return POLL_TIMEOUT_FOREVER;

    if (tv.tv_sec <= 0 && tv.tv_usec <= 0)
        return POLL_TIMEOUT_FOREVER;

    return ((uint64_t)tv.tv_sec * 1000000000ULL) + ((uint64_t)tv.tv_usec * 1000ULL);
}


/**
 * @brief Converts send()/recv() flags from Linux numbering to lwIP's, dropping the ones the kernel handles itself.
 *
 * @param flags The caller's flags.
 * @return The flags for lwIP.
 */
static int __socket_msg_flags(int flags) {

    int r = MSG_DONTWAIT;

    if (flags & LINUX_MSG_OOB)
        r |= MSG_OOB;

    if (flags & LINUX_MSG_PEEK)
        r |= MSG_PEEK;

    if (flags & LINUX_MSG_MORE)
        r |= MSG_MORE;

    return r;
}


/**
 * @brief Receives on a socket for a syscall, waiting for data unless the caller must not block.
 *
 * @param inode The socket inode.
 * @param buf The buffer, in memory the caller has made accessible with uio_lock().
 * @param size The size of @p buf.
 * @param flags MSG_* flags, as Linux numbers them.
 * @param from Receives the sender's address, in kernel memory, or NULL.
 * @param fromlen In/out, the size of @p from.
 * @param nonblock Whether the descriptor is non-blocking.
 * @return The number of bytes received, -EINTR with the syscall marked for restart, or a negative errno.
 */
long socket_recv(inode_t* inode, void* buf, size_t size, int flags, struct sockaddr* from, socklen_t* fromlen, bool nonblock) {

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    int socket = socket_from_inode(inode);

    if (unlikely(socket < 0))
        return -ENOTSOCK;


    volatile uint32_t* word = lwip_socket_event(socket);

    if (unlikely(!word))
        return -EBADF;

    uint32_t seq = *word;

    size_t done = (flags & LINUX_MSG_WAITALL) ? current_task->syscall.progress : 0;

    if (unlikely(done >= size))
        return (long)size;


    ssize_t e;

    if (from)
        e = lwip_recvfrom(socket, (uint8_t*)buf + done, size - done, __socket_msg_flags(flags), from, fromlen);
    else
        e = lwip_recv(socket, (uint8_t*)buf + done, size - done, __socket_msg_flags(flags));


    if (e == 0)
        return (long)done;

    if (e > 0) {

        done += (size_t)e;

        if (!(flags & LINUX_MSG_WAITALL) || (flags & LINUX_MSG_PEEK) || done == size)
            return (long)done;

    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {

        return done ? (long)done : -errno;
    }


    if (nonblock || (flags & LINUX_MSG_DONTWAIT))
        return done ? (long)done : -EAGAIN;

    current_task->syscall.progress = done;


    long r = poll_wait_event(word, seq, socket_timeout(inode, false));

    return (r == -EAGAIN && done) ? (long)done : r;
}


/**
 * @brief Sends on a socket for a syscall, waiting for room until everything is sent unless the caller must not block.
 *
 * @param inode The socket inode.
 * @param buf The data, in memory the caller has made accessible with uio_lock().
 * @param size How many bytes to send.
 * @param flags MSG_* flags, as Linux numbers them.
 * @param to The destination, in kernel memory, or NULL.
 * @param tolen The length of @p to.
 * @param nonblock Whether the descriptor is non-blocking.
 * @return The number of bytes sent, -EINTR with the syscall marked for restart, or a negative errno.
 */
long socket_sendto(inode_t* inode, const void* buf, size_t size, int flags, const struct sockaddr* to, socklen_t tolen, bool nonblock) {

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    int socket = socket_from_inode(inode);

    if (unlikely(socket < 0))
        return -ENOTSOCK;


    volatile uint32_t* word = lwip_socket_event(socket);

    if (unlikely(!word))
        return -EBADF;

    uint32_t seq = *word;

    size_t done = current_task->syscall.progress;

    if (unlikely(done >= size))
        return (long)size;


    ssize_t e = socket_send(socket, (const uint8_t*)buf + done, size - done, __socket_msg_flags(flags), to, tolen);

    if (e >= 0) {

        done += (size_t)e;

        if (done == size)
            return (long)done;

    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {

        return done ? (long)done : -errno;
    }


    if (nonblock || (flags & LINUX_MSG_DONTWAIT))
        return done ? (long)done : -EAGAIN;

    current_task->syscall.progress = done;


    long r = poll_wait_event(word, seq, socket_timeout(inode, true));

    return (r == -EAGAIN && done) ? (long)done : r;
}


/**
 * @brief Accepts a connection for a syscall, waiting for one unless the caller must not block.
 *
 * @param inode The listening socket's inode.
 * @param peer Receives the peer's address, in kernel memory, or NULL.
 * @param peerlen In/out, the size of @p peer.
 * @param nonblock Whether the descriptor is non-blocking.
 * @return The new lwIP socket, -EINTR with the syscall marked for restart, or a negative errno.
 */
long socket_accept(inode_t* inode, struct sockaddr* peer, socklen_t* peerlen, bool nonblock) {

    int socket = socket_from_inode(inode);

    if (unlikely(socket < 0))
        return -ENOTSOCK;


    volatile uint32_t* word = lwip_socket_event(socket);

    if (unlikely(!word))
        return -EBADF;

    uint32_t seq = *word;

    int e = lwip_accept(socket, peer, peerlen);

    if (e >= 0)
        return e;

    if ((errno != EAGAIN && errno != EWOULDBLOCK) || nonblock)
        return -errno;

    return poll_wait_event(word, seq, socket_timeout(inode, false));
}


/**
 * @brief Connects a socket for a syscall, waiting for the handshake unless the caller must not block.
 *
 * @param inode The socket inode.
 * @param addr The address to connect to, in kernel memory.
 * @param len The length of @p addr.
 * @param nonblock Whether the descriptor is non-blocking.
 * @return 0, -EINTR with the syscall marked for restart, or a negative errno.
 */
long socket_connect(inode_t* inode, const struct sockaddr* addr, socklen_t len, bool nonblock) {

    int socket = socket_from_inode(inode);

    if (unlikely(socket < 0))
        return -ENOTSOCK;


    volatile uint32_t* word = lwip_socket_event(socket);

    if (unlikely(!word))
        return -EBADF;

    uint32_t seq = *word;

    if (!current_task->syscall.started) {

        if (lwip_connect(socket, addr, len) == 0)
            return 0;

        if (errno != EINPROGRESS && errno != EALREADY)
            return -errno;

        if (nonblock)
            return -errno;

        current_task->syscall.started = true;
    }


    struct pollfd sfd = {.fd = socket, .events = POLLOUT, .revents = 0};

    if (unlikely(lwip_poll_from_syscall(&sfd, 1) < 0))
        return -errno;


    if (sfd.revents & (POLLOUT | POLLERR | POLLHUP)) {

        current_task->syscall.started = false;


        int err          = 0;
        socklen_t errlen = sizeof(err);

        if (lwip_getsockopt(socket, SOL_SOCKET, SO_ERROR, &err, &errlen) < 0)
            return -errno;

        return -err;
    }


    long r = poll_wait_event(word, seq, socket_timeout(inode, true));

    return r == -EAGAIN ? -EINPROGRESS : r;
}


/**
 * @brief Turns off a lingering close on a socket, so that closing it never waits for the peer.
 *
 * @param socket The lwIP socket.
 */
void socket_unlinger(int socket) {

    struct linger lg = {0};
    socklen_t len    = sizeof(lg);

    if (lwip_getsockopt(socket, SOL_SOCKET, SO_LINGER, &lg, &len) < 0)
        return;

    if (!lg.l_onoff || lg.l_linger == 0)
        return;

    lg.l_onoff = 0;

    lwip_setsockopt(socket, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
}


static int sockfs_close(inode_t* inode) {

    DEBUG_ASSERT(inode);


    int socket = socket_from_inode(inode);

    if (unlikely(socket < 0))
        return 0;


    inode->userdata = NULL;

    socket_unlinger(socket);

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


    if (unlikely(lwip_socket_nonblocking(socket) < 0)) {

        lwip_close(socket);
        return -EBADF;
    }


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
