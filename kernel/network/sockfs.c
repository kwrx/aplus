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
#include <stdio.h>
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


#define SOCKFS_ENCODE(s) ((void*)(uintptr_t)((s) + 1))
#define SOCKFS_DECODE(p) ((int)(uintptr_t)(p) - 1)


#define LINUX_MSG_OOB      0x0001
#define LINUX_MSG_PEEK     0x0002
#define LINUX_MSG_DONTWAIT 0x0040
#define LINUX_MSG_WAITALL  0x0100
#define LINUX_MSG_MORE     0x8000


int socket_from_inode(inode_t* inode) {

    if (unlikely(!inode))
        return -1;

    if (inode->sb != &sockfs_superblock)
        return -1;

    return SOCKFS_DECODE(inode->userdata);
}


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


#define SOCKFS_CHUNK_MAX ((8 * PML1_PAGESIZE) - 64)


static bool __socket_is_stream(int socket) {

    int type      = 0;
    socklen_t len = sizeof(type);

    return lwip_getsockopt(socket, SOL_SOCKET, SO_TYPE, &type, &len) == 0 && type == SOCK_STREAM;
}


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


#define SOCKFS_SIOCGIFCONF    0x8912
#define SOCKFS_SIOCGIFFLAGS   0x8913
#define SOCKFS_SIOCGIFADDR    0x8915
#define SOCKFS_SIOCGIFBRDADDR 0x8919
#define SOCKFS_SIOCGIFNETMASK 0x891B
#define SOCKFS_SIOCGIFMTU     0x8921
#define SOCKFS_SIOCGIFHWADDR  0x8927
#define SOCKFS_SIOCGIFINDEX   0x8933

#define SOCKFS_IFNAMSIZ      16
#define SOCKFS_IFCONF_MAX    8
#define SOCKFS_IFF_UP        0x0001
#define SOCKFS_IFF_BROADCAST 0x0002
#define SOCKFS_IFF_LOOPBACK  0x0008
#define SOCKFS_IFF_RUNNING   0x0040
#define SOCKFS_IFF_MULTICAST 0x1000
#define SOCKFS_ARPHRD_ETHER  1
#define SOCKFS_ARPHRD_LOOP   772


struct sockfs_ifreq {

    char name[SOCKFS_IFNAMSIZ];

    union {

        struct {
            uint16_t family;
            uint16_t port;
            uint32_t addr;
            uint8_t zero[8];
        } in;

        struct {
            uint16_t family;
            uint8_t data[14];
        } hw;

        int16_t flags;
        int32_t value;

        struct {
            unsigned long mem_start;
            unsigned long mem_end;
            unsigned short base_addr;
            unsigned char irq;
            unsigned char dma;
            unsigned char port;
        } map;
    };
};


struct sockfs_ifconf {

    int len;

    union {
        char* buf;
        struct sockfs_ifreq* req;
    };
};


static bool sockfs_user(const void* p, size_t size, int mode) {
    return p && size && uio_check(p, mode) && uio_check((const uint8_t*)p + size - 1, mode);
}


static void sockfs_ifname(const struct netif* netif, char* out) {
    snprintf(out, SOCKFS_IFNAMSIZ, "%c%c%u", netif->name[0], netif->name[1], (unsigned)netif->num);
}


static int sockfs_ioctl_ifconf(void* arg) {

    struct sockfs_ifconf conf;
    struct sockfs_ifreq reqs[SOCKFS_IFCONF_MAX];

    if (unlikely(!sockfs_user(arg, sizeof(conf), R_OK | W_OK)))
        return -EFAULT;

    uio_memcpy_u2s(&conf, arg, sizeof(conf));

    memset(reqs, 0, sizeof(reqs));

    size_t count = 0;

    LOCK_TCPIP_CORE();

    struct netif* netif;

    NETIF_FOREACH(netif) {

        if (count == SOCKFS_IFCONF_MAX)
            break;

        sockfs_ifname(netif, reqs[count].name);

        reqs[count].in.family = AF_INET;
        reqs[count].in.addr   = ip4_addr_get_u32(netif_ip4_addr(netif));

        count++;
    }

    UNLOCK_TCPIP_CORE();


    if (conf.buf) {

        size_t fit = conf.len > 0 ? (size_t)conf.len / sizeof(reqs[0]) : 0;

        if (fit > count)
            fit = count;

        if (fit && unlikely(!sockfs_user(conf.buf, fit * sizeof(reqs[0]), W_OK)))
            return -EFAULT;

        if (fit)
            uio_memcpy_s2u(conf.buf, reqs, fit * sizeof(reqs[0]));

        count = fit;
    }

    conf.len = (int)(count * sizeof(reqs[0]));

    uio_memcpy_s2u(arg, &conf, sizeof(conf));

    return 0;
}


static int sockfs_ioctl_ifreq(long req, void* arg) {

    struct sockfs_ifreq r;

    if (unlikely(!sockfs_user(arg, sizeof(r), R_OK | W_OK)))
        return -EFAULT;

    uio_memcpy_u2s(&r, arg, sizeof(r));

    r.name[SOCKFS_IFNAMSIZ - 1] = '\0';


    LOCK_TCPIP_CORE();

    struct netif* netif = netif_find(r.name);

    if (unlikely(!netif)) {
        UNLOCK_TCPIP_CORE();
        return -ENODEV;
    }

    const bool loopback = netif->name[0] == 'l' && netif->name[1] == 'o';

    const uint32_t ip   = ip4_addr_get_u32(netif_ip4_addr(netif));
    const uint32_t mask = ip4_addr_get_u32(netif_ip4_netmask(netif));

    memset(&r.map, 0, sizeof(r.map));

    switch (req) {

        case SOCKFS_SIOCGIFFLAGS:
            r.flags = (int16_t)((netif_is_up(netif) ? SOCKFS_IFF_UP : 0) | (netif_is_link_up(netif) ? SOCKFS_IFF_RUNNING : 0) | ((netif->flags & NETIF_FLAG_BROADCAST) ? SOCKFS_IFF_BROADCAST : 0) | (loopback ? SOCKFS_IFF_LOOPBACK : 0) |
                                ((netif->flags & NETIF_FLAG_IGMP) ? SOCKFS_IFF_MULTICAST : 0));
            break;

        case SOCKFS_SIOCGIFADDR:
            r.in.family = AF_INET;
            r.in.addr   = ip;
            break;

        case SOCKFS_SIOCGIFNETMASK:
            r.in.family = AF_INET;
            r.in.addr   = mask;
            break;

        case SOCKFS_SIOCGIFBRDADDR:
            r.in.family = AF_INET;
            r.in.addr   = ip | ~mask;
            break;

        case SOCKFS_SIOCGIFMTU:
            r.value = netif->mtu;
            break;

        case SOCKFS_SIOCGIFHWADDR:
            r.hw.family = loopback ? SOCKFS_ARPHRD_LOOP : SOCKFS_ARPHRD_ETHER;
            memcpy(r.hw.data, netif->hwaddr, netif->hwaddr_len < sizeof(r.hw.data) ? netif->hwaddr_len : sizeof(r.hw.data));
            break;

        case SOCKFS_SIOCGIFINDEX:
            r.value = netif_get_index(netif);
            break;
    }

    UNLOCK_TCPIP_CORE();


    uio_memcpy_s2u(arg, &r, sizeof(r));

    return 0;
}


static int sockfs_ioctl(inode_t* inode, long req, void* arg) {

    DEBUG_ASSERT(inode);


    switch (req) {

        case SOCKFS_SIOCGIFCONF:
            return sockfs_ioctl_ifconf(arg);

        case SOCKFS_SIOCGIFFLAGS:
        case SOCKFS_SIOCGIFADDR:
        case SOCKFS_SIOCGIFBRDADDR:
        case SOCKFS_SIOCGIFNETMASK:
        case SOCKFS_SIOCGIFMTU:
        case SOCKFS_SIOCGIFHWADDR:
        case SOCKFS_SIOCGIFINDEX:
            return sockfs_ioctl_ifreq(req, arg);
    }


    int socket = socket_from_inode(inode);

    if (unlikely(socket < 0))
        return -EIO;


    int e = lwip_ioctl(socket, req, arg);

    if (unlikely(e < 0))
        return -errno;

    return e;
}


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


volatile uint32_t* socket_event_word(inode_t* inode) {

    int socket = socket_from_inode(inode);

    if (socket < 0)
        return NULL;

    return (volatile uint32_t*)lwip_socket_event(socket);
}


int socket_poll_arm(inode_t* inode, int events, struct timespec* timeout) {

    __unused_param(events);


    volatile uint32_t* word = socket_event_word(inode);

    if (!word)
        return 0;

    futex_wait(current_task, word, *word, timeout);

    return 1;
}


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


    struct file* ref = fd_append(inode, 0, O_RDWR | (flags & O_NONBLOCK));

    if (unlikely(!ref)) {

        inode->userdata = NULL;
        vfs_anonymous_free(inode);

        lwip_close(socket);
        return -ENFILE;
    }


    int fd          = -EMFILE;
    const int limit = fd_limit();

    shared_ptr_access(current_task->fd, fds, {
        scoped_lock(&current_task->lock) {
            for (int i = 0; i < limit; i++) {

                if (fds->descriptors[i].ref)
                    continue;

                fds->descriptors[i].ref           = ref;
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
