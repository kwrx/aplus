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
 * @brief Entries are keyed by a pointer into the cached inode's own name buffer.
 *
 * The map is told nothing about key ownership, so it neither copies nor frees a key; the
 * key stays valid exactly as long as the inode it points into is in the map.
 */
void vfs_dcache_init(inode_t* inode) {
    hashmap_init(&inode->dcache, hashmap_hash_string, strcmp);
}

void vfs_dcache_free(inode_t* inode) {
    hashmap_cleanup(&inode->dcache);
}


/**
 * @brief Cache @p inode under its own name in @p parent, and return the entry now cached.
 *
 * The returned inode is not necessarily the one passed in. Two CPUs can miss the cache for
 * one name and both walk the filesystem for it, and on a filesystem that builds an inode
 * per lookup (ext2, iso9660) they arrive here holding two distinct objects for one file.
 * Whichever gets in first wins and every later caller is handed the winner, so a path never
 * resolves to a second inode carrying its own lock behind the back of the first. This used
 * to assert that the name was absent instead, which is the panic seen on more than one CPU.
 *
 * The loser is left for the caller to deal with: the VFS cannot tell whether it owns that
 * inode. tmpfs and procfs hand back the same object on every lookup, so for them the loser
 * *is* the winner and freeing it would corrupt the filesystem.
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


    //? An insertion that could not allocate leaves the inode uncached rather than
    //? unreachable: the caller still gets a usable inode, the next lookup just walks again.
    return cached ? cached : inode;
}


/**
 * @brief Drop the entry for @p name from @p parent and free the inode behind it.
 *
 * Keyed by name rather than by inode because the caller used to have to look the entry up
 * and hand the result straight back, which dereferenced NULL for any name that was not
 * cached. Doing both halves here also keeps them atomic against a lookup on another CPU.
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
 * @brief Look @p name up in @p parent's cache, or NULL if it is not cached.
 *
 * Callable without the parent's inode->lock held, which is what vfs_finddir() does on its
 * hot path, so this cannot borrow that lock for consistency: an insertion on another CPU
 * rehashes and frees the table a lockless reader is probing. The map's own lock -- taken by
 * every entry point in this file -- is what makes a lookup on one CPU safe against an
 * insertion or an eviction on another.
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
