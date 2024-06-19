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
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/smp.h>
#include <aplus/hal.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(CONFIG_HAVE_NETWORK)
#include <aplus/network.h>
#else
struct sockaddr;
typedef uint32_t socklen_t;
#endif


/***
 * Name:        recvfrom
 * Description: receive a message from a socket
 * URL:         http://man7.org/linux/man-pages/man2/recvfrom.2.html
 *
 * Input Parameters:
 *  0: 0x2d
 *  1: undefined
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

struct sockaddr;

SYSCALL(45, recvfrom,
long sys_recvfrom (int fd, void __user * buf, size_t size, unsigned flags, struct sockaddr __user * sockaddr, socklen_t __user * socklen) {

#if defined(CONFIG_HAVE_NETWORK)

    if(unlikely(!NETWORK_IS_SOCKFD(fd)))
        return -ENOTSOCK;

    if(unlikely(!buf))
        return -EINVAL;

    if(unlikely(!uio_check(buf, R_OK | W_OK)))
        return -EFAULT;

    if(unlikely(sockaddr && !socklen))
        return -EINVAL;

    if(unlikely(sockaddr && !uio_check(sockaddr, R_OK | W_OK)))
        return -EFAULT;

    if(unlikely(sockaddr && !uio_check(socklen, R_OK | W_OK)))
        return -EFAULT;

    if(unlikely(!size))
        return 0;


    ssize_t e;



    uio_lock(buf, size);

    if(likely(sockaddr)) {

        socklen_t __socklen = uio_r32(socklen);

        char __sockaddr[__socklen];
        uio_memcpy_u2s(__sockaddr, sockaddr, __socklen);

        e = lwip_recvfrom(NETWORK_SOCKFD(fd), buf, size, flags, (struct sockaddr*) __sockaddr, &__socklen);

        uio_w32(socklen, __socklen);
        uio_memcpy_s2u(sockaddr, __sockaddr, __socklen);

    } else {

        e = lwip_recv(NETWORK_SOCKFD(fd), buf, size, flags);

    }

    uio_unlock(buf, size);

    

    if(unlikely(e < 0))
        return -errno;

    return e;

#else
    return -ENOSYS;
#endif

});
