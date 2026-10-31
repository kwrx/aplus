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

#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/vfs.h>

#include "sysfs.h"


inode_t* sysfs_root_finddir(inode_t* inode, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_SYSFS);
    DEBUG_ASSERT(inode->sb->root == inode);
    DEBUG_ASSERT(name);

    if (strcmp(name, "bus") == 0)
        return sysfs_service_bus_inode(inode);

    if (strcmp(name, "devices") == 0)
        return sysfs_service_devices_inode(inode);

    return errno = ENOENT, NULL;
}
