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

#include <sys/mount.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include "ext2.h"


static void ext2_release_tree(inode_t* dir, struct superblock* sb) {
    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(sb);

    if (!dir->dcache.map_base.table) {
        return;
    }

    inode_t* child;
    hashmap_foreach_data(child, &dir->dcache) {

        if (child->sb != sb) {
            continue;
        }

        DEBUG_ASSERT((atomic_load(&child->refcount) & ~0x80000000U) == 0);

        ext2_release_tree(child, sb);

        vfs_dcache_free(child);
        kfree(child);
    }
}


int ext2_umount(inode_t* dir) {
    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(dir->sb);
    DEBUG_ASSERT(dir->sb->fsid == FSID_EXT2);
    DEBUG_ASSERT(dir->sb->root == dir);

    ext2_t* ext2 = (ext2_t*)dir->sb->fsinfo;

    ext2_release_tree(dir, dir->sb);
    cache_destroy(&dir->sb->cache);
    cache_destroy(&ext2->bcache);

    for (size_t i = 0; i < EXT2_IND_LEVELS; i++) {
        if (ext2->mapcache[i].pointers) {
            kfree(ext2->mapcache[i].pointers);
        }
    }

    if (ext2->balloc.bitmap) {
        kfree(ext2->balloc.bitmap);
    }
    
    kfree(ext2->iocache);
    kfree(ext2);
    return 0;
}
