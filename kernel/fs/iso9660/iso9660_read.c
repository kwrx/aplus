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
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <aplus/errno.h>
#include <aplus/endian.h>

#include "iso9660.h"



ssize_t iso9660_read(inode_t* inode, void* buf, off_t pos, size_t len) {
   
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_ISO9660);

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(len);


    iso9660_t* iso9660 = (iso9660_t*) inode->sb->fsinfo;


    iso9660_directory_record_t* record = NULL;
    
    if(unlikely(inode == inode->sb->root)) {

        record = &iso9660->root;

    } else {

        iso9660_inode_t* e = cache_get(&inode->sb->cache, inode->userdata);

        if(unlikely(!e))
            return errno = EIO, -1;

        record = &e->record;

    }



    size_t position = le32_to_cpu(record->extent_location.lsb) * iso9660->block_size;
    size_t size     = le32_to_cpu(record->data_length.lsb);


    if(unlikely(pos >= size))
        return 0;

    if(unlikely(pos + len > size))
        len = size - pos;

    if(unlikely(!len))
        return 0;


    return vfs_read(inode->sb->dev, buf, position + pos, len);
    
}