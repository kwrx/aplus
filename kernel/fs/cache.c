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
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/errno.h>

#include <aplus/utils/list.h>


__returns_nonnull
static void* __unsafe_vfs_get_inode(vfs_cache_t* cache, ino_t ino) {
    
    if(!cache->count)
        goto alloc;


    list_each(cache->items, i) {
        
        if(likely(i->ino != ino))
            continue;

        return i->data;
    }


alloc:

    if(cache->count == cache->capacity)
        kpanicf("__unsafe_vfs_get_inode(): PANIC! Cache Overflow!\n");


    struct vfs_cache_item* i = (struct vfs_cache_item*) kcalloc(sizeof(struct vfs_cache_item), 1, GFP_KERNEL);

    i->ino = ino;
    i->data = cache->ops.load(cache, ino);

    list_push(cache->items, i);


    return cache->count++
         , i->data;
}


__returns_nonnull
void* vfs_cache_get(vfs_cache_t* cache, ino_t ino) {

    __lock_return(&cache->lock, void*,
        __unsafe_vfs_get_inode(cache, ino);
    );

}


void vfs_cache_flush(vfs_cache_t* cache, ino_t ino) {

    if(!cache->count)
        return;


    list_each(cache->items, i) {
        
        if(likely(i->ino != ino))
            continue;

        __lock(&cache->lock, cache->ops.flush(cache, i->ino, i->data));
        break;
    }

}


void vfs_cache_flush_all(vfs_cache_t* cache) {

    list_each(cache->items, i) {  

        __lock(&cache->lock, cache->ops.flush(cache, i->ino, i->data));
        
    }

}


void vfs_cache_create(vfs_cache_t* cache, struct vfs_cache_ops* ops, int capacity, void* userdata) {
   
    DEBUG_ASSERT(cache);
    DEBUG_ASSERT(ops);
    DEBUG_ASSERT(ops->load);
    DEBUG_ASSERT(ops->flush);
    DEBUG_ASSERT(capacity);


    memset(cache, 0, sizeof(vfs_cache_t));
    memcpy(&cache->ops, ops, sizeof(struct vfs_cache_ops));

    cache->capacity = capacity;
    cache->count    = 0;
    cache->userdata = userdata;

    spinlock_init(&cache->lock);

}


void vfs_cache_destroy(vfs_cache_t* cache) {

    DEBUG_ASSERT(cache);

    
    list_each(cache->items, i) {

        if(unlikely(!i->data))
            continue;


        cache->ops.flush(cache, i->ino, i->data);

        kfree(i->data);
    }

    list_clear(cache->items);


    cache->count = 0;
}