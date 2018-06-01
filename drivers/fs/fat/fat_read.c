/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "fat.h"


int fat_read(struct inode* inode, void* ptr, off_t pos, size_t len) {
    if(unlikely(!inode))
        return 0;

    if(unlikely(!ptr))
        return 0;

    if(unlikely(!inode->userdata))
        return 0;

    if(pos + len > inode->size)
        len = inode->size - pos;

    if(unlikely(!len))
        return 0;


    fat_t* fat = (fat_t*) inode->userdata;
    uint32_t cidx = (pos / FAT_SECTOR_SIZE) / fat->sb->sectors_per_cluster;
    uint32_t sect = (pos / FAT_SECTOR_SIZE) - (cidx * fat->sb->sectors_per_cluster);

            
}