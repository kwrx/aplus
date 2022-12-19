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

#include "ext2.h"


// TODO: rewrite all ext2 cache functions

struct ext2_inode* ext2_icache_fetch(cache_t* cache, ext2_t* ext2, ino_t ino) {

    struct ext2_inode* i = (struct ext2_inode*) kcalloc(sizeof(struct ext2_inode), 1, GFP_KERNEL);

    if(unlikely(!i))
        return NULL;

    return ext2_utils_read_inode(ext2, ino, i), i;

}


void ext2_icache_commit(cache_t* cache, ext2_t* ext2, ino_t ino, struct ext2_inode* inode) {
    //ext2_utils_write_inode(ext2, ino, inode);
}

void ext2_icache_release(cache_t* cache, ext2_t* ext2, ino_t ino, struct ext2_inode* inode) {
    kfree(inode);
}



void* ext2_bcache_fetch(cache_t* cache, ext2_t* ext2, uint64_t block) {

    void* buffer = kmalloc(ext2->blocksize, GFP_KERNEL);

    if(unlikely(!buffer))
        return NULL;

    if(unlikely(vfs_read(ext2->dev, buffer, block * ext2->blocksize, ext2->blocksize) != ext2->blocksize)) {
        return kfree(buffer), NULL;
    }

    return buffer;

}


void ext2_bcache_commit(cache_t* cache, ext2_t* ext2, uint64_t block, void* buffer) {
    // vfs_write(ext2->dev, buffer, block * ext2->blocksize, ext2->blocksize);
}

void ext2_bcache_release(cache_t* cache, ext2_t* ext2, uint64_t block, void* buffer) {
    kfree(buffer);
}

