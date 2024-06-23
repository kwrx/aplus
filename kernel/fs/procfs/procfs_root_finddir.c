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

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/types.h>


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include "procfs.h"


inode_t *procfs_root_finddir(inode_t *inode, const char *name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(inode->sb->root == inode);
    DEBUG_ASSERT(name);


    if (isdigit(name[0])) {

        long pid = atol(name);

        if (pid > 0) {
            return procfs_service_pid_inode(inode, pid);
        }

    } else {

        if (strcmp(name, "self") == 0)
            return procfs_service_pid_inode(inode, 0);

        // if(strcmp(name, "cpuinfo") == 0)
        //     return procfs_cpuinfo_inode();

        if (strcmp(name, "meminfo") == 0)
            return procfs_service_meminfo_inode(inode);

        if (strcmp(name, "uptime") == 0)
            return procfs_service_uptime_inode(inode);

        // if(strcmp(name, "version") == 0)
        //     return procfs_version_inode();

        // if(strcmp(name, "filesystems") == 0)
        //     return procfs_filesystems_inode();

        if (strcmp(name, "cmdline") == 0)
            return procfs_service_cmdline_inode(inode, -1);
    }


    return errno = ENOENT, NULL;
}
