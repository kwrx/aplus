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
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>


SYSCALL(122, uname,
int sys_uname(struct utsname* buf) {
    if(unlikely(!buf)) {
        errno = EFAULT;
        return -1;
    }
    
    
    char* __hostname = hostname;


    int fd = sys_open("/etc/hostname", O_RDONLY, 0666);
    if(likely(fd >= 0)) {
        char buf[64];
        memset(buf, 0, sizeof(buf));
        
        sys_read(fd, buf, sizeof(buf));
        sys_close(fd);
        
        __hostname = buf;
    }

    strcpy(buf->sysname, KERNEL_NAME);
    strcpy(buf->nodename, __hostname);
    strcpy(buf->release, KERNEL_VERSION);
    strcpy(buf->version, KERNEL_CODENAME);
    strcpy(buf->machine, KERNEL_PLATFORM);

    return 0;
});
