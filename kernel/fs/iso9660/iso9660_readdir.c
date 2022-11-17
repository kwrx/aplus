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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/smp.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <aplus/errno.h>
#include <aplus/endian.h>
#include <stdint.h>

#include <aplus/utils/list.h>

#include "iso9660.h"




ssize_t iso9660_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->dev);
    DEBUG_ASSERT(inode->sb->fsinfo);
    DEBUG_ASSERT(inode->sb->fsid == FSID_ISO9660);
    
    DEBUG_ASSERT(e);

    if(unlikely(count == 0))
        return 0;
 
 

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
    size_t limit    = le32_to_cpu(record->data_length.lsb) + position;


    size_t index = 0;


    do {

        uint8_t length = 0;

        if(unlikely(vfs_read(inode->sb->dev, &length, position, sizeof(uint8_t)) != sizeof(uint8_t)))
            return errno = EIO, -1;


        if(unlikely(length > 0)) {

            iso9660_inode_t* child;

            if(unlikely((child = cache_get(&inode->sb->cache, position)) == NULL))
                break;

            if(unlikely(pos-- > 0))
                goto next;
            

            e[index].d_ino = child->st.st_ino;
            e[index].d_off = pos;
            e[index].d_reclen = sizeof(struct dirent);
            e[index].d_type = child->st.st_mode & S_IFMT;

            strncpy(e[index].d_name, child->name, sizeof(e[index].d_name));


            if(unlikely(++index >= count))
                break;

next:

            position += child->record.length;   

        } else {

            position += iso9660->block_size;
            position &= ~(iso9660->block_size - 1);

        }

    } while(position < limit);


    return index;
    
}