/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <lwip/sockets.h>
#include <libc.h>

ssize_t sys_recvfrom(int fd, void *restrict buffer, size_t len, int flags, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
#if CONFIG_NETWORK
    return lwip_recvfrom(fd - TASK_FD_COUNT, buffer, len, flags, addr, addrlen);
#endif

    errno = ENOSYS;
    return -1;
}

SYSCALL(712, _recvfrom,
ssize_t sys__recvfrom(uintptr_t* p) {
    return sys_recvfrom(
        (int) p[0],
        (void*) p[1],
        (size_t) p[2],
        (int) p[3],
        (struct sockaddr*) p[4],
        (socklen_t*) p[5]
    );
});

EXPORT(sys_recvfrom);