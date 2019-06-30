/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <aplus/debug.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <stdint.h>


void root_init(void) {

    DEBUG_ASSERT(mbd->cmdline);


    char root[MAXNAMLEN] = { 0 };
    char rootfs[MAXNAMLEN] = { 0 };


    char* p;
    if((p = strstr(mbd->cmdline, "root=")))
        sscanf(p, "root=%s", root);
    else
        kpanic("root: FAIL! 'root' param not found: %s", mbd->cmdline);


    if((p = strstr(mbd->cmdline, "rootfs=")))
        sscanf(p, "rootfs=%s", rootfs);
    else
        kpanic("root: FAIL! 'rootfs' param not found: %s", mbd->cmdline);



    int e;
    if((e = sys_mkdir("/root", 0644)) < 0)
        kpanic("root: FAIL! mkdir() failed: %s", strerror(-e));

    if((e = sys_mount(root, "/root", rootfs, 0, NULL)) < 0)
        kpanic("root: FAIL! mount() failed: %s", strerror(-e));

    if((e = sys_chroot("/root")) < 0)
        kpanic("root: FAIL! chroot() failed: %s", strerror(-e));
    

    /* TODO: parse /etc/fstab */
}