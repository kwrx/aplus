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
#include <stdio.h>
#include <sys/mount.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>

#include "procfs.h"


static int procfs_service_uptime_fetch(inode_t *inode, char **buf, size_t *size, void *arg) {

    (void)arg;

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(inode->sb->root == inode);

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);

    static char buffer[64] = {0};


    *size = snprintf(buffer, sizeof(buffer), "%lu.%lu %lu.%lu", (unsigned long)(arch_timer_generic_getms() / 1000), (unsigned long)(arch_timer_generic_getms() % 1000), (unsigned long)(arch_timer_generic_getms() / 1000),
                     (unsigned long)(arch_timer_generic_getms() % 1000));

    *buf = buffer;

    return 0;
}

inode_t *procfs_service_uptime_inode(inode_t *parent) {

    static inode_t *inode = NULL;

    if (inode == NULL) {
        inode = procfs_service_inode(parent, "uptime", S_IFREG | 0666, procfs_service_uptime_fetch, NULL);
    }

    return inode;
}
