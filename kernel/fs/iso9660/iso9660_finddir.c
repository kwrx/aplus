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

#include <aplus/utils/list.h>

#include "iso9660.h"





inode_t* iso9660_finddir(inode_t* inode, const char* name) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->dev);
    DEBUG_ASSERT(inode->sb->fsinfo);
    DEBUG_ASSERT(inode->sb->fsid == FSID_ISO9660);
    DEBUG_ASSERT(name);

    
    iso9660_t* iso9660 = (iso9660_t*) inode->sb->fsinfo;


    iso9660_directory_record_t* record = NULL;
    
    if(unlikely(inode == inode->sb->root)) {

        record = &iso9660->root;

    } else {

        iso9660_inode_t* e = cache_get(&inode->sb->cache, inode->userdata);

        if(unlikely(!e))
            return errno = EIO, NULL;

        record = &e->record;

    }



    size_t position = le32_to_cpu(record->extent_location.lsb) * iso9660->block_size;
    size_t limit    = le32_to_cpu(record->data_length.lsb) + position;


    do {

        uint8_t length = 0;

        if(unlikely(vfs_read(inode->sb->dev, &length, position, sizeof(uint8_t)) != sizeof(uint8_t)))
            return errno = EIO, NULL;


        if(unlikely(length > 0)) {

            iso9660_inode_t* child;

            if(unlikely((child = cache_get(&inode->sb->cache, position)) == NULL))
                break;


            if(unlikely(strncmp(child->name, name, ISO9660_MAX_NAME) == 0)) {

                
                inode_t* d = (inode_t*) kcalloc(sizeof(inode_t), 1, GFP_KERNEL);

                d->ino    = child->st.st_ino;
                d->sb     = inode->sb;
                d->parent = inode;
                
                strncpy(d->name, child->name, CONFIG_MAXNAMLEN);


                d->ops.getattr = iso9660_getattr;

                if(S_ISDIR(child->st.st_mode)) {

                    d->ops.finddir = iso9660_finddir;
                    d->ops.readdir = iso9660_readdir;

                    vfs_dcache_init(d);

                }

                else if(S_ISREG(child->st.st_mode)) {

                    d->ops.read  = iso9660_read;

                }

                else if(S_ISLNK(child->st.st_mode)) {

                    d->ops.readlink = iso9660_readlink;

                }

                d->userdata = (void*) ((uintptr_t) position);
                    

                spinlock_init(&d->lock);

                return d;

            }


            position += child->record.length;

        } else {

            position += iso9660->block_size;
            position &= ~(iso9660->block_size - 1);

        }

    } while(position < limit);
    

    return errno = ENOENT, NULL;

}