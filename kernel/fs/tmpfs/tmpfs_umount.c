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
#include <sys/mount.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include <aplus/utils/list.h>

#include "tmpfs.h"



int tmpfs_umount(inode_t* dir) {

    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(dir->sb);
    DEBUG_ASSERT(dir->sb->fsid == FSID_TMPFS);
    DEBUG_ASSERT(dir->sb->root == dir);


    tmpfs_t* tmpfs = (tmpfs_t*)dir->sb->fsinfo;


    cache_destroy(&dir->sb->cache);

    list_each(tmpfs->children, i) {

        if (i->userdata) {
            kfree(i->userdata);
        }

        kfree(i);
    }

    list_clear(tmpfs->children);


    kfree(tmpfs);

    return 0;
}
