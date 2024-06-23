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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>



void root_init(void) {


    char cmd[CONFIG_BUFSIZ]       = {0};
    char root[CONFIG_MAXNAMLEN]   = {0};
    char rootfs[CONFIG_MAXNAMLEN] = {0};


    strncpy(cmd, core->boot.cmdline, sizeof(cmd));


    char* tok = cmd;

    for (char* s = strtok_r(cmd, " ", &tok); s; s = strtok_r(NULL, " ", &tok)) {

        if ((strstr(s, "root="))) {
            strncpy(root, &s[5], CONFIG_MAXNAMLEN);
        }

        if ((strstr(s, "rootfs="))) {
            strncpy(rootfs, &s[7], CONFIG_MAXNAMLEN);
        }

        // TODO: add other Kernel Arguments
    }



    if (root[0] == '\0') {
        kpanicf("root: PANIC! 'root' param not found: %s\n", core->boot.cmdline);
    }

    if (rootfs[0] == '\0') {
        kpanicf("root: PANIC! 'rootfs' param not found: %s\n", core->boot.cmdline);
    }



    int e;

    if ((e = sys_mkdir("/root", 0644)) < 0)
        kpanicf("root: PANIC! mkdir() failed: errno(%s)\n", strerror(-e));

    if ((e = sys_mount(root, "/root", rootfs, 0, NULL)) < 0)
        kpanicf("root: PANIC! mount %s in %s failed: errno(%s)\n", "/root", root, strerror(-e));

    if ((e = sys_mount("/dev", "/root/dev", "bind", 0, NULL)) < 0)
        kpanicf("root: PANIC! mount %s in %s failed: errno(%s)\n", "/root/dev", "/dev", strerror(-e));

    if ((e = sys_chroot("/root")) < 0)
        kpanicf("root: PANIC! chroot() failed: errno(%s)\n", strerror(-e));


#if DEBUG_LEVEL_TRACE
    kprintf("root: rootfs mounted on /root (root=%s, rootfs=%s)\n", root, rootfs);
#endif
}
