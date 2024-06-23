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



void vfs_dcache_init(inode_t *inode) {
    hashmap_init(&inode->dcache, hashmap_hash_string, strcmp);
}

void vfs_dcache_free(inode_t *inode) {
    hashmap_cleanup(&inode->dcache);
}


void vfs_dcache_add(inode_t *parent, inode_t *inode) {

    if (inode->flags & INODE_FLAGS_DCACHE_DISABLED)
        return;

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->name);
    DEBUG_ASSERT(inode->name[0] != '\0');
    DEBUG_ASSERT(hashmap_get(&parent->dcache, inode->name) == NULL);

    hashmap_put(&parent->dcache, inode->name, inode);
}


void vfs_dcache_remove(inode_t *parent, inode_t *inode) {

    if (inode->flags & INODE_FLAGS_DCACHE_DISABLED)
        return;

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->name);
    DEBUG_ASSERT(inode->name[0] != '\0');
    DEBUG_ASSERT(hashmap_get(&parent->dcache, inode->name) != NULL);

    kfree(hashmap_remove(&parent->dcache, inode->name));
}


inode_t *vfs_dcache_find(inode_t *parent, const char *name) {

    if (parent->flags & INODE_FLAGS_DCACHE_DISABLED)
        return NULL;

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(name[0] != '\0');

    return hashmap_get(&parent->dcache, name);
}
