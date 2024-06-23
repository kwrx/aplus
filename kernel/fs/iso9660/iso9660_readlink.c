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
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include "iso9660.h"



ssize_t iso9660_readlink(inode_t* inode, char* buf, size_t len) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_ISO9660);



    if (unlikely(len > ISO9660_MAX_SYMLINK))
        len = ISO9660_MAX_SYMLINK;

    if (unlikely(len == 0))
        return 0;

    if (unlikely(inode == inode->sb->root))
        return errno = ENOLINK, -1;



    iso9660_inode_t* e = cache_get(&inode->sb->cache, inode->userdata);

    if (unlikely(!e))
        return errno = EIO, -1;


    strncpy(buf, e->symlink, len);

    return len;
}
