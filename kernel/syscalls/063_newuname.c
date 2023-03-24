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


#include <stdint.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <aplus/hal.h>


#define __NEW_UTS_LEN 64

struct new_utsname {
	char sysname[__NEW_UTS_LEN + 1];
	char nodename[__NEW_UTS_LEN + 1];
	char release[__NEW_UTS_LEN + 1];
	char version[__NEW_UTS_LEN + 1];
	char machine[__NEW_UTS_LEN + 1];
	char domainname[__NEW_UTS_LEN + 1];
};


// See kernel/init/hostname.c
extern char* hostname;




/***
 * Name:        newuname
 * Description: 
 * URL:         http://man7.org/linux/man-pages/man2/newuname.2.html
 *
 * Input Parameters:
 *  0: 0x3f
 *  1: struct new_utsname __user * name
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */
SYSCALL(63, newuname,
long sys_newuname (struct new_utsname __user * name) {
    
    DEBUG_ASSERT(hostname != NULL);

    if(unlikely(!name))
        return -EFAULT;

    if(unlikely(!uio_check(name, R_OK | W_OK)))
        return -EFAULT;


    struct new_utsname utsname = {
        .sysname = CONFIG_SYSTEM_NAME,
        .release = CONFIG_SYSTEM_VERSION,
        .version = CONFIG_SYSTEM_CODENAME,
        .machine = CONFIG_COMPILER_HOST,
        .domainname = "(none)"
    };

    strncpy(utsname.nodename, hostname, sizeof(utsname.nodename));

    uio_memcpy_s2u(name, &utsname, sizeof(struct new_utsname));

    return 0;

});
