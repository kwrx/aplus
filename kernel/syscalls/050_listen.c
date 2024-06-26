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
#endif


/***
 * Name:        listen
 * Description: listen for connections on a socket
 * URL:         http://man7.org/linux/man-pages/man2/listen.2.html
 *
 * Input Parameters:
 *  0: 0x32
 *  1: int
 *  2: int
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    50, listen, long sys_listen(int fd, int backlog) {

#if defined(CONFIG_HAVE_NETWORK)
        if (unlikely(!NETWORK_IS_SOCKFD(fd)))
            return -ENOTSOCK;


        ssize_t e;

        if ((e = lwip_listen(NETWORK_SOCKFD(fd), backlog)) < 0)
            return -errno;

        return e;

#else
    return -ENOSYS;
#endif
    });
