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

#ifndef _APLUS_UNIX_H
#define _APLUS_UNIX_H

#ifndef __ASSEMBLY__

    #include <stdint.h>

    #include <aplus.h>
    #include <aplus/ipc.h>
    #include <aplus/vfs.h>

    #include <aplus/utils/ringbuffer.h>


/**
 * @brief The local address family, which lwIP's headers do not carry; must match the libc's PF_LOCAL.
 */
    #define AF_UNIX_LOCAL 1

    #define UNIX_PATH_MAX 108


/**
 * @brief The local address, spelled out here because lwIP owns struct sockaddr and has no sockaddr_un.
 */

struct sockaddr_un_k {

    uint16_t sun_family;
    char sun_path[UNIX_PATH_MAX];
};


/**
 * @brief The shutdown directions as a bitmask, which POSIX's SHUT_RD of 0 cannot be stored as.
 */
    #define UNIX_SHUT_RD 1
    #define UNIX_SHUT_WR 2


    #define UNIX_SOCK_UNBOUND   0
    #define UNIX_SOCK_BOUND     1
    #define UNIX_SOCK_LISTENING 2
    #define UNIX_SOCK_CONNECTED 3
    #define UNIX_SOCK_CLOSED    4


/**
 * @brief Matches SOCK_STREAM in lwIP and in the libc userspace is built against.
 */
    #define UNIX_TYPE_STREAM 1


    #define UNIX_SOCK_BUFSIZ  CONFIG_PIPESIZ
    #define UNIX_SOCK_BACKLOG 128


/**
 * @brief One endpoint of a local socket; a connected pair is two of these pointing at each other.
 *
 * An endpoint outlives the descriptor that closed it, since the peer holds a reference to it.
 */

struct unix_sock {

    int type;
    int state;

    atomic_int refcount;

    ringbuffer_t rx;

    struct unix_sock* peer;

    //? Connections that have been queued by connect() but not yet claimed by
    //? accept(). Only ever non-NULL on a listening socket.
    struct unix_sock** backlog;
    size_t backlog_len;
    size_t backlog_cap;

    //? Shared with the endpoint inode, so a write on one side wakes whoever is
    //? parked on the other.
    inode_events_t ev;

    //? The S_IFSOCK directory entry bind() attached this socket to, if any.
    inode_t* bound;

    int shutdown_flags;

    spinlock_t lock;
};


__BEGIN_DECLS

/**
 * @brief Resolves a descriptor to a local socket, or NULL when it is not one.
 */
struct unix_sock* unix_sock_from_fd(int fd);

long unix_socket(int type, int protocol);
long unix_socketpair(int type, int protocol, int* sv);

/**
 * @brief Addresses are taken as opaque user pointers, so this interface need not agree with lwIP's.
 */
long unix_bind(struct unix_sock* sock, const void* addr, uint32_t len);
long unix_listen(struct unix_sock* sock, int backlog);
long unix_connect(struct unix_sock* sock, const void* addr, uint32_t len);
long unix_accept(struct unix_sock* sock, void* addr, uint32_t* len, int flags);
long unix_shutdown(struct unix_sock* sock, int how);

/**
 * @brief Registers the caller on this socket's change counter, which only records the intent to sleep.
 */
void unix_sock_wait(struct unix_sock* sock);
long unix_getsockname(struct unix_sock* sock, void* addr, uint32_t* len);
long unix_getpeername(struct unix_sock* sock, void* addr, uint32_t* len);

__END_DECLS

#endif
#endif
