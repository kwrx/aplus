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

#ifndef _APLUS_EPOLL_H
#define _APLUS_EPOLL_H

#ifndef __ASSEMBLY__

    #include <stdint.h>

    #include <aplus.h>
    #include <aplus/vfs.h>


    #define EPOLL_KCTL_ADD 1
    #define EPOLL_KCTL_DEL 2
    #define EPOLL_KCTL_MOD 3

    #define EPOLL_KONESHOT (1U << 30)
    #define EPOLL_KET      (1U << 31)

    #define EPOLL_KMAXEVENTS (INT32_MAX / 12)


struct epoll_kevent {
    uint32_t events;
    uint64_t data;
} __packed;


__BEGIN_DECLS

inode_t* epoll_inode(void);
bool epoll_is(inode_t* inode);

long epoll_ctl(inode_t* inode, int op, int fd, struct file* target, const struct epoll_kevent* event);
long epoll_wait(inode_t* inode, struct epoll_kevent* events, int maxevents, uint64_t timeout_ns);

long epoll_create_fd(bool cloexec);
long epoll_ctl_fd(int epfd, int op, int fd, const void* uevent);
long epoll_wait_fd(int epfd, void* uevents, int maxevents, uint64_t timeout_ns);

__END_DECLS

#endif
#endif
