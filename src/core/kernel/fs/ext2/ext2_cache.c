/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <aplus/smp.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <stdint.h>
#include <aplus/errno.h>

#include "ext2.h"



void* ext2_cache_load(vfs_cache_t* cache, ino_t ino) {

    struct ext2_inode* i = (struct ext2_inode*) kcalloc(sizeof(struct ext2_inode), 1, GFP_KERNEL);

    ext2_utils_read_inode((ext2_t*) cache->userdata, ino, i);

    return (void*) i;
}



void ext2_cache_flush(vfs_cache_t* cache, ino_t ino, void* data) {

    //ext2_utils_write_inode((ext2_t*) cache->userdata, ino, data);

    return;
}