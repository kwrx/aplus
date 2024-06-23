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
 * Name:        sendto
 * Description: send a message on a socket
 * URL:         http://man7.org/linux/man-pages/man2/sendto.2.html
 *
 * Input Parameters:
 *  0: 0x2c
 *  1: undefined
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    44, sendto, long sys_sendto(int fd, const void* buf, size_t size, unsigned flags, const struct sockaddr* sockaddr, socklen_t socklen) {

#if defined(CONFIG_HAVE_NETWORK)
        if (unlikely(!NETWORK_IS_SOCKFD(fd)))
            return -ENOTSOCK;

        if (unlikely(!buf))
            return -EINVAL;

        if (unlikely(!uio_check(buf, R_OK)))
            return -EFAULT;

        if (unlikely(sockaddr && !socklen))
            return -EINVAL;

        if (unlikely(sockaddr && !uio_check(sockaddr, R_OK)))
            return -EFAULT;

        if (unlikely(!size))
            return 0;


        ssize_t e;


        uio_lock(buf, size);

        if (likely(sockaddr)) {

            char __sockaddr[socklen];
            uio_memcpy_u2s(__sockaddr, sockaddr, socklen);

            e = lwip_sendto(NETWORK_SOCKFD(fd), buf, size, flags, (struct sockaddr*)__sockaddr, socklen);

        } else {

            e = lwip_send(NETWORK_SOCKFD(fd), buf, size, flags);
        }

        uio_unlock(buf, size);



        if (unlikely(e < 0))
            return -errno;

        return e;

#else
    return -ENOSYS;
#endif
    });
