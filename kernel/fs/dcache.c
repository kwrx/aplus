/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>

#include <aplus/utils/hashmap.h>



/**
 * @brief Prepares the directory entry cache of an inode.
 *
 * @param inode The inode whose cache is initialised.
 */
void vfs_dcache_init(inode_t* inode) {
    hashmap_init(&inode->dcache, hashmap_hash_string, strcmp);
}

void vfs_dcache_free(inode_t* inode) {
    hashmap_cleanup(&inode->dcache);
}


/**
 * @brief Caches an inode under its own name in a parent directory.
 *
 * @param parent The directory the entry is cached in.
 * @param inode The inode to cache.
 * @return The inode now cached under that name, which is an earlier one if a lookup raced this call.
 */
inode_t* vfs_dcache_add(inode_t* parent, inode_t* inode) {

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->name[0] != '\0');

    if (inode->flags & INODE_FLAGS_DCACHE_DISABLED)
        return inode;


    inode_t* cached = NULL;

    hashmap_lock(&parent->dcache);
    {
        if ((cached = hashmap_get(&parent->dcache, inode->name)) == NULL) {

            if (likely(hashmap_put(&parent->dcache, inode->name, inode) == 0))
                cached = inode;
        }
    }
    hashmap_unlock(&parent->dcache);


    return cached ? cached : inode;
}


/**
 * @brief Drops a cached entry from a directory and frees the inode behind it.
 *
 * @param parent The directory holding the entry.
 * @param name The name of the entry to drop.
 */
void vfs_dcache_remove(inode_t* parent, const char* name) {

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(name[0] != '\0');

    if (parent->flags & INODE_FLAGS_DCACHE_DISABLED)
        return;


    inode_t* inode = NULL;

    hashmap_lock(&parent->dcache);
    {
        inode = hashmap_remove(&parent->dcache, name);
    }
    hashmap_unlock(&parent->dcache);


    if (likely(inode))
        kfree(inode);
}


/**
 * @brief Looks a name up in a directory's entry cache.
 *
 * @param parent The directory to search.
 * @param name The name to look for.
 * @return The cached inode, or NULL if the name is not cached.
 */
inode_t* vfs_dcache_find(inode_t* parent, const char* name) {

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(name[0] != '\0');

    if (parent->flags & INODE_FLAGS_DCACHE_DISABLED)
        return NULL;


    inode_t* inode = NULL;

    hashmap_lock(&parent->dcache);
    {
        inode = hashmap_get(&parent->dcache, name);
    }
    hashmap_unlock(&parent->dcache);

    return inode;
}
