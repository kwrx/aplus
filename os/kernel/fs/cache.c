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
#include <aplus/mm.h>
#include <aplus/vfs.h>
#include <stdint.h>


static inode_t* __unsafe_vfs_get_inode(vfs_cache_t* cache, ino_t ino) {
    
    if(!cache->count)
        goto alloc;


    int i;
    for(i = 0; i < cache->capacity; i++) {

        if(!cache->inodes[i])
            continue;


        DEBUG_ASSERT(cache->inodes[i]->ino);

        if(cache->inodes[i]->ino->st.st_ino == ino)
            return cache->inodes[i];

    }

alloc:

    if(cache->count == cache->capacity) {

        ktime_t oldest = (ktime_t) ~0;

        int i, j;
        for(i = 0, j = 0; i < cache->capacity; i++) {

            if(oldest < cache->times[i])
                continue;

            oldest = cache->times[i];
            j = i;
        }

        if(cache->inodes[j])
            kfree(cache->inodes[j]);

        
        cache->inodes[j] = kcalloc(sizeof(inode_t), 1, GFP_KERNEL);
        cache->times[j]  = arch_timer_getticks();

        return cache->inodes[j]->ino = PTR_REF(&cache->inodes[j]->__ino)
             , cache->inodes[j]->ino->st.st_ino = ino
             , cache->inodes[j];
    
    } else {

        int i;
        for(i = 0; i < cache->capacity; i++) {

            if(cache->inodes[i])
                continue;


            cache->count++;
            
            cache->inodes[i] = kcalloc(sizeof(inode_t), 1, GFP_KERNEL);
            cache->times[i]  = arch_timer_getticks();

            return cache->inodes[j]->ino = PTR_REF(&cache->inodes[j]->__ino)
                 , cache->inodes[i]->ino->st.st_ino = ino
                 , cache->inodes[i];
    
        }

    }


    kpanic("%s(): BUG! %s:%d", __func__, __FILE__, __LINE__);
}


inode_t* vfs_cache_get_inode(vfs_cache_t* cache, ino_t ino) {

    __lock_return(&cache->lock, inode_t*,
        __unsafe_vfs_get_inode(cache, ino);
    );

}



void vfs_cache_create(vfs_cache_t** cache, int capacity) {
   
    DEBUG_ASSERT(cache);
    DEBUG_ASSERT((*cache) == NULL);

    DEBUG_ASSERT(capacity);
    DEBUG_WARNING(capacity < 4096 && "capacity is too high!");


    (*cache) = kcalloc(sizeof(vfs_cache_t), 1, GFP_KERNEL);
    (*cache)->inodes = kcalloc(sizeof(inode_t*), capacity, GFP_KERNEL);
    (*cache)->times = kcalloc(sizeof(ktime_t), capacity, GFP_KERNEL);

    (*cache)->capacity = capacity;
    (*cache)->count = 0;

    spinlock_init(&(*cache)->lock);

}


void vfs_cache_destroy(vfs_cache_t** cache) {

    DEBUG_ASSERT(cache);
    DEBUG_ASSERT((*cache) != NULL);

    
    int i;
    for(i = 0; i < (*cache)->capacity; i++) {
        if(!(*cache)->inodes[i])
            continue;

        if(--(*cache)->inodes[i]->ino->refcount == 0)
            if((*cache)->inodes[i]->userdata)
                kfree((*cache)->inodes[i]->userdata);

        kfree((*cache)->inodes[i]);
    }


    kfree((*cache)->inodes);
    kfree((*cache)->times);
    kfree((*cache));

    (*cache) = NULL;

}