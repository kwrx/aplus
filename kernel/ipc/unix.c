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
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/unix.h>
#include <aplus/vfs.h>


/**
 * @brief Local sockets are ordinary VFS descriptors backed by an inode, not a numbering space of their own.
 *
 * dup(), fork() inheritance, close-on-exec, poll() and the last-close teardown all work unchanged.
 */


#define SOCKFS_FSID      0xDEADB0CE
#define SOCKFS_FIRST_INO 0xFFFFFFFFE000000


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


static void __unix_put(struct unix_sock* sock);


static void __unix_wake(struct unix_sock* sock) {

    if (unlikely(!sock))
        return;

    shared_ptr_nullable_access(sock->ev, ev, {
        atomic_fetch_add(&ev->futex, 1);
    });
}


/**
 * @brief Tells whether a socket's peer is gone, so that nothing more will ever arrive.
 *
 * @param sock The endpoint to test.
 * @return true if the peer has been released or closed.
 */

static inline bool __unix_peer_gone(struct unix_sock* sock) {

    return sock->peer == NULL || sock->peer->state == UNIX_SOCK_CLOSED;
}

static inline bool __unix_recv_done(struct unix_sock* sock) {

    return __unix_peer_gone(sock) || (sock->peer->shutdown_flags & UNIX_SHUT_WR);
}

static inline bool __unix_send_done(struct unix_sock* sock) {

    return __unix_peer_gone(sock) || (sock->peer->shutdown_flags & UNIX_SHUT_RD);
}


static struct unix_sock* __unix_get(struct unix_sock* sock) {

    if (likely(sock))
        atomic_fetch_add(&sock->refcount, 1);

    return sock;
}


static void __unix_put(struct unix_sock* sock) {

    if (unlikely(!sock))
        return;

    if (atomic_fetch_sub(&sock->refcount, 1) != 1)
        return;


    ringbuffer_destroy(&sock->rx);

    if (sock->backlog)
        kfree(sock->backlog);

    kfree(sock);
}


static struct unix_sock* __unix_alloc(int type) {

    struct unix_sock* sock = kcalloc(1, sizeof(struct unix_sock), GFP_KERNEL);

    if (unlikely(!sock))
        return NULL;

    if (unlikely(ringbuffer_init(&sock->rx, UNIX_SOCK_BUFSIZ) < 0)) {

        kfree(sock);
        return NULL;
    }

    sock->ev = shared_ptr_new(struct inode_events, GFP_KERNEL);

    if (unlikely(!sock->ev)) {

        ringbuffer_destroy(&sock->rx);
        kfree(sock);

        return NULL;
    }

    sock->type  = type;
    sock->state = UNIX_SOCK_UNBOUND;

    atomic_store(&sock->refcount, 1);
    spinlock_init(&sock->lock);

    return sock;
}


static ssize_t sockfs_read(inode_t* inode, void* buf, off_t offset, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);

    __unused_param(offset);

    struct unix_sock* sock = (struct unix_sock*)inode->userdata;

    if (unlikely(!sock))
        return -EIO;

    if (unlikely(sock->state == UNIX_SOCK_LISTENING))
        return -EINVAL;

    if (unlikely(sock->state != UNIX_SOCK_CONNECTED))
        return -ENOTCONN;

    if (unlikely(size == 0))
        return 0;

    if (unlikely(sock->shutdown_flags & UNIX_SHUT_RD))
        return 0;


    ssize_t e = ringbuffer_read(&sock->rx, buf, size);

    if (e == -EAGAIN && __unix_recv_done(sock))
        return 0;

    if (e > 0)
        __unix_wake(sock->peer);

    return e;
}


static ssize_t sockfs_write(inode_t* inode, const void* buf, off_t offset, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);

    __unused_param(offset);

    struct unix_sock* sock = (struct unix_sock*)inode->userdata;

    if (unlikely(!sock))
        return -EIO;

    if (unlikely(sock->state != UNIX_SOCK_CONNECTED))
        return -ENOTCONN;

    if (unlikely(size == 0))
        return 0;

    if (unlikely(sock->shutdown_flags & UNIX_SHUT_WR))
        return -EPIPE;

    if (unlikely(__unix_send_done(sock)))
        return -EPIPE;


    ssize_t e = ringbuffer_write(&sock->peer->rx, buf, size);

    if (e > 0)
        __unix_wake(sock->peer);

    return e;
}


static int sockfs_poll(inode_t* inode, int events) {

    DEBUG_ASSERT(inode);

    struct unix_sock* sock = (struct unix_sock*)inode->userdata;

    if (unlikely(!sock))
        return POLLERR;


    int revents = 0;

    if (sock->state == UNIX_SOCK_LISTENING) {

        if (sock->backlog_len > 0)
            revents |= POLLIN;

        return revents & (events | POLLHUP | POLLERR);
    }


    if (sock->state != UNIX_SOCK_CONNECTED)
        return (sock->state == UNIX_SOCK_CLOSED ? POLLHUP : 0) & (events | POLLHUP | POLLERR);


    if (ringbuffer_available(&sock->rx) > 0 || __unix_recv_done(sock))
        revents |= POLLIN;

    if (sock->shutdown_flags & UNIX_SHUT_RD)
        revents |= POLLIN;

    if (__unix_peer_gone(sock))
        revents |= POLLHUP;

    else if (!__unix_send_done(sock) && ringbuffer_writeable(&sock->peer->rx) > 0)
        revents |= POLLOUT;

    return revents & (events | POLLHUP | POLLERR);
}


static int sockfs_close(inode_t* inode) {

    DEBUG_ASSERT(inode);

    struct unix_sock* sock = (struct unix_sock*)inode->userdata;

    if (unlikely(!sock))
        return 0;

    inode->userdata = NULL;


    struct unix_sock* peer    = NULL;
    struct unix_sock** queued = NULL;
    size_t queued_len         = 0;

    scoped_lock(&sock->lock) {

        sock->state = UNIX_SOCK_CLOSED;

        peer = sock->peer;

        queued          = sock->backlog;
        queued_len      = sock->backlog_len;
        sock->backlog   = NULL;
        sock->backlog_len = 0;

        if (sock->bound) {

            sock->bound->userdata = NULL;
            sock->bound           = NULL;
        }
    }


    __unix_wake(sock);
    __unix_wake(peer);


    if (queued) {

        for (size_t i = 0; i < queued_len; i++) {

            if (!queued[i])
                continue;

            queued[i]->state = UNIX_SOCK_CLOSED;

            __unix_wake(queued[i]->peer);
            __unix_put(queued[i]);
        }

        kfree(queued);
    }


    __unix_put(peer);
    __unix_put(sock);

    return 0;
}


static int sockfs_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(st);

    struct unix_sock* sock = (struct unix_sock*)inode->userdata;

    st->st_ino     = inode->ino;
    st->st_mode    = S_IFSOCK | 0666;
    st->st_nlink   = 1;
    st->st_size    = sock ? (off_t)ringbuffer_available(&sock->rx) : 0;
    st->st_blksize = CONFIG_BUFSIZ;

    st->st_mtime = arch_timer_gettime();
    st->st_atime = arch_timer_gettime();
    st->st_ctime = arch_timer_gettime();

    return 0;
}


static inode_t* __sockfs_inode(struct unix_sock* sock) {

    inode_t* inode = kcalloc(1, sizeof(inode_t), GFP_KERNEL);

    if (unlikely(!inode))
        return NULL;

    inode->name[0] = '\0';
    inode->ino     = __sockfs_next_ino++;
    inode->sb      = &sockfs_superblock;
    inode->parent  = NULL;
    inode->flags   = INODE_FLAGS_ANONYMOUS;

    inode->userdata = __unix_get(sock);

    inode->ops.close   = sockfs_close;
    inode->ops.read    = sockfs_read;
    inode->ops.write   = sockfs_write;
    inode->ops.poll    = sockfs_poll;
    inode->ops.getattr = sockfs_getattr;

    inode->ev = shared_ptr_ref(sock->ev);

    spinlock_init(&inode->lock);

    return inode;
}


/**
 * @brief Installs an endpoint into the caller's descriptor table.
 *
 * @param sock The endpoint to install.
 * @param flags O_NONBLOCK and O_CLOEXEC as requested by the caller.
 * @return The new descriptor, or a negative errno with everything it allocated released again.
 */

static long __unix_install(struct unix_sock* sock, int flags) {

    inode_t* inode = __sockfs_inode(sock);

    if (unlikely(!inode))
        return -ENOMEM;


    struct file* ref = fd_append(inode, 0, 0);

    if (unlikely(!ref)) {

        inode->userdata = NULL;
        kfree(inode);

        __unix_put(sock);

        return -ENFILE;
    }


    long fd = -EMFILE;

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


struct unix_sock* unix_sock_from_fd(int fd) {

    if (unlikely(fd < 0 || fd >= CONFIG_OPEN_MAX))
        return NULL;


    struct unix_sock* sock = NULL;

    shared_ptr_access(current_task->fd, fds, {
        if (fds->descriptors[fd].ref != NULL && fds->descriptors[fd].ref->inode != NULL && fds->descriptors[fd].ref->inode->sb == &sockfs_superblock) {

            sock = (struct unix_sock*)fds->descriptors[fd].ref->inode->userdata;
        }
    });

    return sock;
}


/**
 * @brief Copies a sockaddr_un in from userspace and hands back the path it names.
 *
 * @param addr The caller's address, in user memory.
 * @param len The length of the caller's address.
 * @param out Receives the path.
 * @param outsz The size of @p out.
 * @return 0 on success, or a negative errno.
 */

static long __unix_addr(const void* addr, uint32_t len, char* out, size_t outsz) {

    if (unlikely(!addr))
        return -EFAULT;

    if (unlikely(len < (uint32_t)(sizeof(uint16_t) + 1)))
        return -EINVAL;

    if (unlikely(len > (uint32_t)sizeof(struct sockaddr_un_k)))
        return -EINVAL;

    if (unlikely(!uio_check(addr, R_OK)))
        return -EFAULT;


    struct sockaddr_un_k un = {0};
    uio_memcpy_u2s(&un, addr, len);

    if (unlikely(un.sun_family != AF_UNIX_LOCAL))
        return -EAFNOSUPPORT;


    size_t pathlen = len - sizeof(uint16_t);

    if (unlikely(pathlen == 0 || un.sun_path[0] == '\0'))
        return -EINVAL;

    if (unlikely(pathlen >= outsz))
        return -ENAMETOOLONG;


    memcpy(out, un.sun_path, pathlen);
    out[pathlen] = '\0';

    return 0;
}


static long __unix_addr_out(const char* path, void* addr, uint32_t* len) {

    if (!addr || !len)
        return 0;

    if (unlikely(!uio_check(len, R_OK | W_OK)))
        return -EFAULT;

    uint32_t avail = uio_r32(len);

    if (unlikely(avail && !uio_check(addr, W_OK)))
        return -EFAULT;


    struct sockaddr_un_k un = {0};

    un.sun_family = AF_UNIX_LOCAL;
    strncpy(un.sun_path, path ? path : "", sizeof(un.sun_path) - 1);

    uint32_t full = (uint32_t)(sizeof(uint16_t) + strlen(un.sun_path) + 1);
    uint32_t copy = full < avail ? full : avail;

    if (copy)
        uio_memcpy_s2u(addr, &un, copy);

    uio_w32(len, full);

    return 0;
}


long unix_socket(int type, int protocol) {

    if (unlikely(type != UNIX_TYPE_STREAM))
        return -ESOCKTNOSUPPORT;

    if (unlikely(protocol != 0))
        return -EPROTONOSUPPORT;


    struct unix_sock* sock = __unix_alloc(type);

    if (unlikely(!sock))
        return -ENOMEM;


    long fd = __unix_install(sock, 0);

    if (unlikely(fd < 0))
        __unix_put(sock);

    return fd;
}


long unix_socketpair(int type, int protocol, int* sv) {

    if (unlikely(type != UNIX_TYPE_STREAM))
        return -ESOCKTNOSUPPORT;

    if (unlikely(protocol != 0))
        return -EPROTONOSUPPORT;

    if (unlikely(!sv))
        return -EFAULT;

    if (unlikely(!uio_check(sv, R_OK | W_OK)))
        return -EFAULT;


    struct unix_sock* a = __unix_alloc(type);
    struct unix_sock* b = __unix_alloc(type);

    if (unlikely(!a || !b)) {

        __unix_put(a);
        __unix_put(b);

        return -ENOMEM;
    }


    a->peer  = __unix_get(b);
    b->peer  = __unix_get(a);
    a->state = UNIX_SOCK_CONNECTED;
    b->state = UNIX_SOCK_CONNECTED;


    long fd0 = __unix_install(a, 0);

    if (unlikely(fd0 < 0)) {

        __unix_put(a);
        __unix_put(b);

        return fd0;
    }

    long fd1 = __unix_install(b, 0);

    if (unlikely(fd1 < 0)) {

        sys_close(fd0);
        __unix_put(b);

        return fd1;
    }


    __unix_put(a);
    __unix_put(b);

    uio_w32(&sv[0], (uint32_t)fd0);
    uio_w32(&sv[1], (uint32_t)fd1);

    return 0;
}


long unix_bind(struct unix_sock* sock, const void* addr, uint32_t len) {

    DEBUG_ASSERT(sock);

    if (unlikely(sock->state != UNIX_SOCK_UNBOUND))
        return -EINVAL;


    char path[CONFIG_PATH_MAX] = {0};

    long e;

    if ((e = __unix_addr(addr, len, path, sizeof(path))) < 0)
        return e;


    inode_t* cwd = NULL;

    shared_ptr_access(current_task->fs, fs, { cwd = fs->cwd; });

    if (unlikely(!cwd))
        return -ENOENT;


    inode_t* node = path_lookup(cwd, path, O_CREAT | O_EXCL, S_IFSOCK | 0666);

    if (unlikely(!node))
        return -errno;


    scoped_lock(&sock->lock) {

        node->userdata = sock;

        sock->bound = node;
        sock->state = UNIX_SOCK_BOUND;
    }

    return 0;
}


long unix_listen(struct unix_sock* sock, int backlog) {

    DEBUG_ASSERT(sock);

    if (unlikely(sock->state != UNIX_SOCK_BOUND && sock->state != UNIX_SOCK_LISTENING))
        return -EINVAL;

    if (backlog <= 0)
        backlog = 1;

    if (backlog > UNIX_SOCK_BACKLOG)
        backlog = UNIX_SOCK_BACKLOG;


    scoped_lock(&sock->lock) {

        if (!sock->backlog) {

            sock->backlog = kcalloc((size_t)backlog, sizeof(struct unix_sock*), GFP_KERNEL);

            if (unlikely(!sock->backlog))
                return -ENOMEM;

            sock->backlog_cap = (size_t)backlog;
            sock->backlog_len = 0;
        }

        sock->state = UNIX_SOCK_LISTENING;
    }

    return 0;
}


long unix_connect(struct unix_sock* sock, const void* addr, uint32_t len) {

    DEBUG_ASSERT(sock);

    if (unlikely(sock->state == UNIX_SOCK_CONNECTED))
        return -EISCONN;

    if (unlikely(sock->state != UNIX_SOCK_UNBOUND && sock->state != UNIX_SOCK_BOUND))
        return -EINVAL;


    char path[CONFIG_PATH_MAX] = {0};

    long e;

    if ((e = __unix_addr(addr, len, path, sizeof(path))) < 0)
        return e;


    inode_t* cwd = NULL;

    shared_ptr_access(current_task->fs, fs, { cwd = fs->cwd; });

    if (unlikely(!cwd))
        return -ENOENT;


    inode_t* node = path_lookup(cwd, path, 0, 0);

    if (unlikely(!node))
        return -ECONNREFUSED;


    struct unix_sock* listener = (struct unix_sock*)node->userdata;

    if (unlikely(!listener || node->sb == &sockfs_superblock))
        return -ECONNREFUSED;

    if (unlikely(listener->state != UNIX_SOCK_LISTENING))
        return -ECONNREFUSED;

    if (unlikely(listener->type != sock->type))
        return -EPROTOTYPE;


    struct unix_sock* server = __unix_alloc(sock->type);

    if (unlikely(!server))
        return -ENOMEM;


    scoped_lock(&listener->lock) {

        if (unlikely(listener->backlog_len >= listener->backlog_cap)) {

            __unix_put(server);
            return -ECONNREFUSED;
        }

        server->peer  = __unix_get(sock);
        sock->peer    = __unix_get(server);
        server->state = UNIX_SOCK_CONNECTED;
        sock->state   = UNIX_SOCK_CONNECTED;

        listener->backlog[listener->backlog_len++] = server;
    }

    __unix_wake(listener);

    return 0;
}


long unix_accept(struct unix_sock* sock, void* addr, uint32_t* len, int flags) {

    DEBUG_ASSERT(sock);

    if (unlikely(sock->state != UNIX_SOCK_LISTENING))
        return -EINVAL;


    struct unix_sock* server = NULL;

    scoped_lock(&sock->lock) {

        if (sock->backlog_len > 0) {

            server = sock->backlog[0];

            for (size_t i = 1; i < sock->backlog_len; i++)
                sock->backlog[i - 1] = sock->backlog[i];

            sock->backlog_len--;
        }
    }


    if (!server)
        return -EAGAIN;


    long fd = __unix_install(server, flags);

    if (unlikely(fd < 0)) {

        scoped_lock(&sock->lock) {
            if (sock->backlog_len < sock->backlog_cap) {

                for (size_t i = sock->backlog_len; i > 0; i--)
                    sock->backlog[i] = sock->backlog[i - 1];

                sock->backlog[0] = server;
                sock->backlog_len++;

                server = NULL;
            }
        }

        if (server)
            __unix_put(server);

        return fd;
    }


    __unix_put(server);

    if (addr && len) {

        long e;

        if ((e = __unix_addr_out(sock->bound ? sock->bound->name : "", addr, len)) < 0)
            return e;
    }

    return fd;
}


void unix_sock_wait(struct unix_sock* sock) {

    DEBUG_ASSERT(sock);

    shared_ptr_nullable_access(sock->ev, ev, {
        futex_wait(current_task, &ev->futex, ev->futex, NULL);
    });
}


long unix_shutdown(struct unix_sock* sock, int how) {

    DEBUG_ASSERT(sock);

    if (unlikely(sock->state != UNIX_SOCK_CONNECTED))
        return -ENOTCONN;


    scoped_lock(&sock->lock) {

        switch (how) {

            case 0:
                sock->shutdown_flags |= UNIX_SHUT_RD;
                break;

            case 1:
                sock->shutdown_flags |= UNIX_SHUT_WR;
                break;

            case 2:
                sock->shutdown_flags |= UNIX_SHUT_RD | UNIX_SHUT_WR;
                break;

            default:
                return -EINVAL;
        }
    }

    __unix_wake(sock);
    __unix_wake(sock->peer);

    return 0;
}


long unix_getsockname(struct unix_sock* sock, void* addr, uint32_t* len) {

    DEBUG_ASSERT(sock);

    return __unix_addr_out(sock->bound ? sock->bound->name : "", addr, len);
}


long unix_getpeername(struct unix_sock* sock, void* addr, uint32_t* len) {

    DEBUG_ASSERT(sock);

    if (unlikely(sock->state != UNIX_SOCK_CONNECTED))
        return -ENOTCONN;

    return __unix_addr_out(sock->peer && sock->peer->bound ? sock->peer->bound->name : "", addr, len);
}
