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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(CONFIG_HAVE_NETWORK)
    #include <aplus/network.h>
#else
struct sockaddr;
typedef uint32_t socklen_t;
#endif

/***
 * Name:        bind
 * Description: bind a name to a socket
 * URL:         http://man7.org/linux/man-pages/man2/bind.2.html
 *
 * Input Parameters:
 *  0: 0x31
 *  1: int
 *  2: struct sockaddr  *
 *  3: int
 *
 * Auto-generated by scripts/gen-syscalls.js
 */


SYSCALL(
    49, bind, long sys_bind(int fd, const struct sockaddr *sockaddr, socklen_t socklen) {

#if defined(CONFIG_HAVE_NETWORK)
        if (unlikely(fd < 0))
            return -EBADF;

        if (unlikely(!NETWORK_IS_SOCKFD(fd)))
            return -ENOTSOCK;

        if (unlikely(!sockaddr))
            return -EINVAL;

        if (unlikely(!socklen))
            return -EINVAL;

        if (unlikely(!uio_check(sockaddr, R_OK | W_OK)))
            return -EFAULT;


        char __sockaddr[socklen];
        uio_memcpy_u2s(__sockaddr, sockaddr, socklen);


        ssize_t e;

        if ((e = lwip_bind(NETWORK_SOCKFD(fd), (struct sockaddr *)__sockaddr, socklen)) < 0)
            return -errno;

        return e;

#else
    return -ENOSYS;
#endif
    });
