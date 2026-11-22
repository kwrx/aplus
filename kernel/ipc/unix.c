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
#include <unistd.h>

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

#include <aplus/network.h>


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

static _Atomic ino64_t __sockfs_next_ino = SOCKFS_FIRST_INO + 1;


struct unix_binding {
    struct unix_binding* next;
    struct superblock* sb;
    ino_t ino;
    struct unix_sock* sock;
};

static struct unix_binding* __unix_bindings = NULL;
static spinlock_t __unix_bindings_lock      = SPINLOCK_INIT;


static void __unix_put(struct unix_sock* sock);
static struct unix_sock* __unix_get(struct unix_sock* sock);
static void __unix_rights_drain(struct unix_sock* sock);


static long __unix_bind_register(inode_t* node, struct unix_sock* sock) {

    struct unix_binding* b = kcalloc(1, sizeof(*b), GFP_KERNEL);

    if (unlikely(!b))
        return -ENOMEM;

    b->sb   = node->sb;
    b->ino  = node->ino;
    b->sock = sock;

    struct unix_binding* stale = NULL;

    scoped_lock(&__unix_bindings_lock) {

        for (struct unix_binding** i = &__unix_bindings; *i; i = &(*i)->next) {

            if ((*i)->sb == b->sb && (*i)->ino == b->ino) {
                stale = *i;
                *i    = stale->next;
                break;
            }
        }

        b->next         = __unix_bindings;
        __unix_bindings = b;
    }

    if (stale) {
        kfree(stale);
    }

    return 0;
}


static void __unix_bind_unregister(struct unix_sock* sock) {

    struct unix_binding* found = NULL;

    scoped_lock(&__unix_bindings_lock) {

        for (struct unix_binding** i = &__unix_bindings; *i; i = &(*i)->next) {

            if ((*i)->sock == sock) {
                found = *i;
                *i    = found->next;
                break;
            }
        }
    }

    if (found) {
        kfree(found);
    }
}


static struct unix_sock* __unix_bind_find(inode_t* node) {

    struct unix_sock* sock = NULL;

    scoped_lock(&__unix_bindings_lock) {

        for (struct unix_binding* i = __unix_bindings; i; i = i->next) {

            if (i->sb == node->sb && i->ino == node->ino) {
                sock = __unix_get(i->sock);
                break;
            }
        }
    }

    return sock;
}


static void __unix_wake(struct unix_sock* sock) {

    if (unlikely(!sock))
        return;

    shared_ptr_nullable_access(sock->ev, ev, {
        atomic_fetch_add(&ev->futex, 1);
    });
}


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


    __unix_rights_drain(sock);

    ringbuffer_destroy(&sock->rx);

    if (sock->backlog)
        kfree(sock->backlog);

    if (sock->ev)
        shared_ptr_free(sock->ev);

    kfree(sock);
}


static void __unix_cred_set(struct unix_sock* sock) {

    sock->cred.pid = (int32_t)current_task->pid;
    sock->cred.uid = (uint32_t)current_task->euid;
    sock->cred.gid = (uint32_t)current_task->egid;
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
    spinlock_init(&sock->rx_lock);

    __unix_cred_set(sock);

    return sock;
}


static void __unix_rights_drop(struct unix_rights* rights) {

    for (size_t i = 0; i < rights->count; i++) {

        if (rights->files[i]) {
            fd_remove(rights->files[i], true);
        }
    }

    kfree(rights);
}


static void __unix_rights_drain(struct unix_sock* sock) {

    struct unix_rights* rights = NULL;

    scoped_lock(&sock->rx_lock) {

        rights = sock->rights;

        sock->rights      = NULL;
        sock->rights_last = NULL;
    }

    while (rights) {

        struct unix_rights* next = rights->next;

        __unix_rights_drop(rights);

        rights = next;
    }
}


static ssize_t __unix_rx_read(struct unix_sock* sock, void* buf, size_t size, struct unix_rights** rights) {

    ssize_t e = -EIO;

    *rights = NULL;

    scoped_lock(&sock->rx_lock) {

        struct unix_rights* r = sock->rights;

        if (r && r->pos - sock->rx_tail < size) {

            const uint64_t end = r->pos + r->len - sock->rx_tail;

            if (size > end) {
                size = (size_t)end;
            }
        }

        e = ringbuffer_read(&sock->rx, buf, size);

        if (e <= 0) {
            break;
        }

        sock->rx_tail += (uint64_t)e;

        if (r && r->pos < sock->rx_tail) {

            sock->rights = r->next;

            if (!sock->rights) {
                sock->rights_last = NULL;
            }

            r->next = NULL;
            *rights = r;
        }
    }

    return e;
}


static ssize_t __unix_rx_write(struct unix_sock* peer, const void* buf, size_t size, struct unix_rights** rights) {

    ssize_t e = -EPIPE;

    scoped_lock(&peer->rx_lock) {

        if (unlikely(peer->state == UNIX_SOCK_CLOSED)) {
            break;
        }

        e = ringbuffer_write(&peer->rx, buf, size);

        if (e <= 0) {
            break;
        }

        if (rights && *rights) {

            struct unix_rights* r = *rights;

            r->pos  = peer->rx_head;
            r->len  = (size_t)e;
            r->next = NULL;

            if (peer->rights_last) {
                peer->rights_last->next = r;
            } else {
                peer->rights = r;
            }

            peer->rights_last = r;

            *rights = NULL;
        }

        peer->rx_head += (uint64_t)e;
    }

    return e;
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


    struct unix_rights* rights;

    ssize_t e = __unix_rx_read(sock, buf, size, &rights);

    if (rights) {
        __unix_rights_drop(rights);
    }

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


    ssize_t e = __unix_rx_write(sock->peer, buf, size, NULL);

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
    inode_t* bound            = NULL;

    scoped_lock(&sock->lock) {

        sock->state = UNIX_SOCK_CLOSED;

        peer = sock->peer;

        queued          = sock->backlog;
        queued_len      = sock->backlog_len;
        sock->backlog   = NULL;
        sock->backlog_len = 0;

        bound       = sock->bound;
        sock->bound = NULL;
    }


    __unix_bind_unregister(sock);

    if (bound) {
        vfs_inode_put(bound);
    }


    __unix_rights_drain(sock);

    __unix_wake(sock);
    __unix_wake(peer);


    if (queued) {

        for (size_t i = 0; i < queued_len; i++) {

            if (!queued[i])
                continue;

            struct unix_sock* client = queued[i]->peer;

            queued[i]->state = UNIX_SOCK_CLOSED;
            queued[i]->peer  = NULL;

            __unix_wake(client);
            __unix_put(client);
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
    inode->ino     = atomic_fetch_add(&__sockfs_next_ino, 1);
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


static long __unix_install(struct unix_sock* sock, int flags) {

    inode_t* inode = __sockfs_inode(sock);

    if (unlikely(!inode))
        return -ENOMEM;


    struct file* ref = fd_append(inode, 0, O_RDWR | (flags & O_NONBLOCK));

    if (unlikely(!ref)) {

        inode->userdata = NULL;
        vfs_anonymous_free(inode);

        __unix_put(sock);

        return -ENFILE;
    }


    long fd = fd_install(ref, !!(flags & O_CLOEXEC));


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

            sock = __unix_get((struct unix_sock*)fds->descriptors[fd].ref->inode->userdata);
        }
    });

    return sock;
}


void unix_sock_put(struct unix_sock* sock) {

    __unix_put(sock);
}


static void __unix_bound_name(struct unix_sock* sock, char* out, size_t size) {

    out[0] = '\0';

    if (!sock)
        return;

    scoped_lock(&sock->lock) {

        strncpy(out, sock->path, size - 1);
        out[size - 1] = '\0';
    }
}


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


static int __unix_type_flags(int* type) {

    int flags = 0;

    if (*type & UNIX_TYPE_NONBLOCK)
        flags |= O_NONBLOCK;

    if (*type & UNIX_TYPE_CLOEXEC)
        flags |= O_CLOEXEC;

    *type &= ~(UNIX_TYPE_NONBLOCK | UNIX_TYPE_CLOEXEC);

    return flags;
}


long unix_socket(int type, int protocol) {

    int flags = __unix_type_flags(&type);

    if (unlikely(type != UNIX_TYPE_STREAM))
        return -ESOCKTNOSUPPORT;

    if (unlikely(protocol != 0))
        return -EPROTONOSUPPORT;


    struct unix_sock* sock = __unix_alloc(type);

    if (unlikely(!sock))
        return -ENOMEM;


    long fd = __unix_install(sock, flags);

    __unix_put(sock);

    return fd;
}


long unix_socketpair(int type, int protocol, int* sv) {

    int flags = __unix_type_flags(&type);

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


    long fd0 = __unix_install(a, flags);

    if (unlikely(fd0 < 0)) {

        __unix_put(a);
        __unix_put(b);

        return fd0;
    }

    long fd1 = __unix_install(b, flags);

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


    inode_t* cwd __scoped(vfs_inode_cleanup) = NULL;

    shared_ptr_access(current_task->fs, fs, {
        if (fs->cwd)
            cwd = vfs_inode_get(fs->cwd);
    });

    if (unlikely(!cwd))
        return -ENOENT;


    inode_t* node __scoped(vfs_inode_cleanup) = path_lookup(cwd, path, O_CREAT | O_EXCL, S_IFSOCK | 0666);

    if (unlikely(!node))
        return -errno;


    if ((e = __unix_bind_register(node, sock)) < 0)
        return e;

    e = -EINVAL;

    scoped_lock(&sock->lock) {

        if (unlikely(sock->state != UNIX_SOCK_UNBOUND))
            break;

        sock->bound = vfs_inode_get(node);
        sock->state = UNIX_SOCK_BOUND;

        strncpy(sock->path, path, sizeof(sock->path) - 1);

        e = 0;
    }

    if (unlikely(e < 0)) {
        __unix_bind_unregister(sock);
    }

    return e;
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

        __unix_cred_set(sock);
    }

    return 0;
}


long unix_connect(struct unix_sock* sock, const void* addr, uint32_t len) {

    DEBUG_ASSERT(sock);

    if (unlikely(sock->state == UNIX_SOCK_CONNECTED))
        return -EISCONN;

    if (unlikely(sock->state != UNIX_SOCK_UNBOUND && sock->state != UNIX_SOCK_BOUND))
        return -EINVAL;

    __unix_cred_set(sock);


    char path[CONFIG_PATH_MAX] = {0};

    long e;

    if ((e = __unix_addr(addr, len, path, sizeof(path))) < 0)
        return e;


    inode_t* cwd __scoped(vfs_inode_cleanup) = NULL;

    shared_ptr_access(current_task->fs, fs, {
        if (fs->cwd)
            cwd = vfs_inode_get(fs->cwd);
    });

    if (unlikely(!cwd))
        return -ENOENT;


    inode_t* node __scoped(vfs_inode_cleanup) = path_lookup(cwd, path, 0, 0);

    if (unlikely(!node)) {
        return errno == EACCES ? -EACCES : -ECONNREFUSED;
    }


    struct stat st;

    if (unlikely(node->sb == &sockfs_superblock || vfs_getattr(node, &st) < 0 || !S_ISSOCK(st.st_mode)))
        return -ECONNREFUSED;

    if ((e = attr_access(node, W_OK)) < 0) {
        return e;
    }


    struct unix_sock* listener = __unix_bind_find(node);

    if (unlikely(!listener))
        return -ECONNREFUSED;


    struct unix_sock* server = NULL;

    if (unlikely(listener->state != UNIX_SOCK_LISTENING))
        e = -ECONNREFUSED;
    else if (unlikely(listener->type != sock->type))
        e = -EPROTOTYPE;
    else if (unlikely(!(server = __unix_alloc(sock->type))))
        e = -ENOMEM;
    else
        e = -ECONNREFUSED;


    if (server) {

        scoped_lock(&listener->lock) {

            if (unlikely(listener->state != UNIX_SOCK_LISTENING || listener->backlog_len >= listener->backlog_cap))
                break;

            server->peer  = __unix_get(sock);
            sock->peer    = __unix_get(server);
            server->cred  = listener->cred;
            server->state = UNIX_SOCK_CONNECTED;
            sock->state   = UNIX_SOCK_CONNECTED;

            memcpy(server->path, listener->path, sizeof(server->path));

            listener->backlog[listener->backlog_len++] = server;
            server = NULL;

            e = 0;
        }

        if (server)
            __unix_put(server);
    }


    if (e == 0)
        __unix_wake(listener);

    __unix_put(listener);

    return e;
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

        char name[UNIX_PATH_MAX];
        __unix_bound_name(sock, name, sizeof(name));

        long e;

        if ((e = __unix_addr_out(name, addr, len)) < 0)
            return e;
    }

    return fd;
}


uint32_t unix_sock_seq(struct unix_sock* sock) {

    DEBUG_ASSERT(sock);

    uint32_t seq = 0;

    shared_ptr_nullable_access(sock->ev, ev, { seq = ev->futex; });

    return seq;
}


void unix_sock_wait(struct unix_sock* sock, uint32_t seq) {

    DEBUG_ASSERT(sock);

    shared_ptr_nullable_access(sock->ev, ev, {
        futex_wait(current_task, &ev->futex, seq, NULL);
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

    char name[UNIX_PATH_MAX];
    __unix_bound_name(sock, name, sizeof(name));

    return __unix_addr_out(name, addr, len);
}


long unix_getpeername(struct unix_sock* sock, void* addr, uint32_t* len) {

    DEBUG_ASSERT(sock);

    if (unlikely(sock->state != UNIX_SOCK_CONNECTED))
        return -ENOTCONN;


    struct unix_sock* peer = NULL;

    scoped_lock(&sock->lock) {
        peer = __unix_get(sock->peer);
    }

    char name[UNIX_PATH_MAX];
    __unix_bound_name(peer, name, sizeof(name));

    __unix_put(peer);

    return __unix_addr_out(name, addr, len);
}


static long __unix_sockopt_out(void* optval, uint32_t* optlen, const void* value, uint32_t size) {

    uint32_t copy = uio_r32(optlen);

    if (copy > size) {
        copy = size;
    }

    if (unlikely(copy && !uio_check(optval, W_OK))) {
        return -EFAULT;
    }

    if (copy) {
        uio_memcpy_s2u(optval, value, copy);
    }

    uio_w32(optlen, copy);

    return 0;
}


long unix_getsockopt(struct unix_sock* sock, int level, int optname, void* optval, uint32_t* optlen) {

    DEBUG_ASSERT(sock);

    if (unlikely(level != UNIX_SOL_SOCKET)) {
        return -ENOPROTOOPT;
    }

    if (unlikely(!optval || !optlen)) {
        return -EFAULT;
    }

    if (unlikely(!uio_check(optlen, R_OK | W_OK))) {
        return -EFAULT;
    }


    int value = 0;

    switch (optname) {

        case UNIX_SO_TYPE:
            value = sock->type;
            break;

        case UNIX_SO_DOMAIN:
            value = UNIX_AF_UNIX;
            break;

        case UNIX_SO_PROTOCOL:
        case UNIX_SO_ERROR:
        case UNIX_SO_REUSEADDR:
        case UNIX_SO_KEEPALIVE:
        case UNIX_SO_PASSCRED:
            value = 0;
            break;

        case UNIX_SO_SNDBUF:
        case UNIX_SO_RCVBUF:
            value = UNIX_SOCK_BUFSIZ;
            break;

        case UNIX_SO_ACCEPTCONN:
            value = sock->state == UNIX_SOCK_LISTENING;
            break;

        case UNIX_SO_PEERCRED: {

            if (unlikely(sock->state != UNIX_SOCK_CONNECTED)) {
                return -ENOTCONN;
            }

            struct unix_sock* peer = NULL;

            scoped_lock(&sock->lock) {
                peer = __unix_get(sock->peer);
            }

            if (unlikely(!peer)) {
                return -ENOTCONN;
            }

            typeof(peer->cred) cred = peer->cred;

            __unix_put(peer);

            return __unix_sockopt_out(optval, optlen, &cred, sizeof(cred));
        }

        default:
            return -ENOPROTOOPT;
    }

    return __unix_sockopt_out(optval, optlen, &value, sizeof(value));
}


long unix_setsockopt(struct unix_sock* sock, int level, int optname, const void* optval, uint32_t optlen) {

    DEBUG_ASSERT(sock);

    if (unlikely(level != UNIX_SOL_SOCKET)) {
        return -ENOPROTOOPT;
    }

    switch (optname) {

        case UNIX_SO_REUSEADDR:
        case UNIX_SO_KEEPALIVE:
        case UNIX_SO_PASSCRED:
        case UNIX_SO_SNDBUF:
        case UNIX_SO_RCVBUF:
            break;

        default:
            return -ENOPROTOOPT;
    }

    if (unlikely(optlen < sizeof(int))) {
        return -EINVAL;
    }

    if (unlikely(!optval || !uio_check(optval, R_OK))) {
        return -EFAULT;
    }

    return 0;
}


static long __unix_rights_import(const void* control, size_t controllen, struct unix_rights** out) {

    *out = NULL;

    size_t count = 0;
    size_t off   = 0;

    while (controllen - off >= sizeof(struct unix_cmsghdr)) {

        const struct unix_cmsghdr* cmsg = (const struct unix_cmsghdr*)((const uint8_t*)control + off);

        if (unlikely(cmsg->len < sizeof(struct unix_cmsghdr) || cmsg->len > controllen - off)) {
            return -EINVAL;
        }

        if (cmsg->level == UNIX_SOL_SOCKET) {

            if (cmsg->type == UNIX_SCM_RIGHTS) {
                count += (cmsg->len - sizeof(struct unix_cmsghdr)) / sizeof(int);
            } else if (cmsg->type != UNIX_SCM_CREDENTIALS) {
                return -EINVAL;
            }
        }

        off += UNIX_CMSG_ALIGN(cmsg->len);

        if (off > controllen) {
            break;
        }
    }

    if (count == 0) {
        return 0;
    }

    if (unlikely(count > UNIX_SCM_MAX_FD)) {
        return -EINVAL;
    }


    struct unix_rights* rights = kcalloc(1, sizeof(struct unix_rights) + count * sizeof(struct file*), GFP_KERNEL);

    if (unlikely(!rights)) {
        return -ENOMEM;
    }


    off = 0;

    while (controllen - off >= sizeof(struct unix_cmsghdr)) {

        const struct unix_cmsghdr* cmsg = (const struct unix_cmsghdr*)((const uint8_t*)control + off);

        if (cmsg->level == UNIX_SOL_SOCKET && cmsg->type == UNIX_SCM_RIGHTS) {

            const size_t n      = (cmsg->len - sizeof(struct unix_cmsghdr)) / sizeof(int);
            const uint8_t* data = (const uint8_t*)(cmsg + 1);

            for (size_t i = 0; i < n; i++) {

                int fd;
                memcpy(&fd, data + i * sizeof(int), sizeof(int));

                struct file* file = fd < 0 ? NULL : fd_get((unsigned int)fd, NULL);

                if (unlikely(!file)) {
                    __unix_rights_drop(rights);
                    return -EBADF;
                }

                rights->files[rights->count++] = file;
            }
        }

        off += UNIX_CMSG_ALIGN(cmsg->len);

        if (off > controllen) {
            break;
        }
    }

    *out = rights;

    return 0;
}


static void __unix_rights_export(struct unix_rights* rights, void* control, size_t room, size_t* used, int* msgflags, bool cloexec) {

    size_t fit = room >= UNIX_CMSG_LEN(sizeof(int)) ? (room - UNIX_CMSG_LEN(0)) / sizeof(int) : 0;

    if (fit > rights->count) {
        fit = rights->count;
    }


    int fds[UNIX_SCM_MAX_FD];
    size_t installed = 0;

    for (size_t i = 0; i < fit; i++) {

        long fd = fd_install(rights->files[i], cloexec);

        if (fd < 0) {
            break;
        }

        fds[installed++] = (int)fd;
        rights->files[i] = NULL;
    }

    if (installed < rights->count) {
        *msgflags |= UNIX_MSG_CTRUNC;
    }

    __unix_rights_drop(rights);


    if (installed == 0) {
        return;
    }

    struct unix_cmsghdr cmsg = {
        .len   = UNIX_CMSG_LEN(installed * sizeof(int)),
        .level = UNIX_SOL_SOCKET,
        .type  = UNIX_SCM_RIGHTS,
    };

    uio_memcpy_s2u(control, &cmsg, sizeof(cmsg));
    uio_memcpy_s2u((uint8_t*)control + sizeof(cmsg), fds, installed * sizeof(int));

    *used = UNIX_CMSG_SPACE(installed * sizeof(int)) < room ? UNIX_CMSG_SPACE(installed * sizeof(int)) : room;
}


long unix_sendmsg(struct unix_sock* sock, const struct iovec* iov, size_t iovcnt, size_t total, const void* control, size_t controllen) {

    DEBUG_ASSERT(sock);


    struct unix_rights* rights = NULL;

    long e = __unix_rights_import(control, controllen, &rights);

    if (unlikely(e < 0)) {
        return e;
    }


    struct unix_sock* peer = NULL;

    scoped_lock(&sock->lock) {
        peer = __unix_get(sock->peer);
    }


    if (unlikely(sock->state != UNIX_SOCK_CONNECTED)) {

        e = -ENOTCONN;

    } else if (unlikely(sock->shutdown_flags & UNIX_SHUT_WR) || unlikely(__unix_send_done(sock)) || unlikely(!peer)) {

        e = -EPIPE;

    } else if (total == 0) {

        e = 0;

    } else {

        size_t n = ringbuffer_writeable(&peer->rx);

        if (n > total) {
            n = total;
        }

        void* kbuf = n ? kmalloc(n, GFP_KERNEL) : NULL;

        if (n == 0) {

            e = -EAGAIN;

        } else if (unlikely(!kbuf)) {

            e = -ENOMEM;

        } else {

            socket_iov_gather(kbuf, iov, iovcnt, n);

            e = __unix_rx_write(peer, kbuf, n, &rights);

            kfree(kbuf);

            if (e > 0) {
                __unix_wake(peer);
            }
        }
    }


    if (rights) {
        __unix_rights_drop(rights);
    }

    __unix_put(peer);

    return e;
}


long unix_recvmsg(struct unix_sock* sock, const struct iovec* iov, size_t iovcnt, size_t total, void* control, size_t* controllen, int* msgflags, int flags) {

    DEBUG_ASSERT(sock);
    DEBUG_ASSERT(controllen);
    DEBUG_ASSERT(msgflags);


    const size_t room = *controllen;

    *controllen = 0;
    *msgflags   = 0;


    if (unlikely(sock->state == UNIX_SOCK_LISTENING)) {
        return -EINVAL;
    }

    if (unlikely(sock->state != UNIX_SOCK_CONNECTED)) {
        return -ENOTCONN;
    }

    if (unlikely(total == 0 || (sock->shutdown_flags & UNIX_SHUT_RD))) {
        return 0;
    }


    size_t n = ringbuffer_available(&sock->rx);

    if (n == 0) {
        return __unix_recv_done(sock) ? 0 : -EAGAIN;
    }

    if (n > total) {
        n = total;
    }


    void* kbuf = kmalloc(n, GFP_KERNEL);

    if (unlikely(!kbuf)) {
        return -ENOMEM;
    }


    struct unix_rights* rights;

    ssize_t e = __unix_rx_read(sock, kbuf, n, &rights);

    if (e > 0) {

        socket_iov_scatter(iov, iovcnt, 0, kbuf, (size_t)e);
        __unix_wake(sock->peer);

    } else if (e == -EAGAIN && __unix_recv_done(sock)) {

        e = 0;
    }

    kfree(kbuf);


    if (rights) {
        __unix_rights_export(rights, control, room, controllen, msgflags, !!(flags & UNIX_MSG_CMSG_CLOEXEC));
    }

    return e;
}
