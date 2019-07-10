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
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <stdint.h>


void aspace_create(address_space_t** aspace, uintptr_t vmmpd, address_space_ops_t* ops, uintptr_t start, uintptr_t end) {

    DEBUG_ASSERT(aspace);
    DEBUG_ASSERT(ops);
    DEBUG_ASSERT(start < end);
    DEBUG_ASSERT((*aspace) == NULL);


    address_space_t* s = (*aspace) = (address_space_t*) kcalloc(sizeof(address_space_t), 1, GFP_KERNEL);

    s->start = start;
    s->end = end;
    s->size = 0;
    s->vmmpd = vmmpd;
    s->refcount = 1;

    s->ops.open   = ops->open;
    s->ops.close  = ops->close;
    s->ops.nopage = ops->nopage;
    
    spinlock_init(&s->lock);

}


void aspace_create_from(address_space_t** aspace, address_space_t* staticbuf, uintptr_t vmmpd, address_space_ops_t* ops, uintptr_t start, uintptr_t end) {

    DEBUG_ASSERT(aspace);
    DEBUG_ASSERT(ops);
    DEBUG_ASSERT(start < end);
    DEBUG_ASSERT((*aspace) == NULL);


    address_space_t* s = (*aspace) = staticbuf;

    s->start = start;
    s->end = end;
    s->size = 0;
    s->vmmpd = vmmpd;
    s->refcount = 1;

    s->ops.open   = ops->open;
    s->ops.close  = ops->close;
    s->ops.nopage = ops->nopage;
    
    spinlock_init(&s->lock);

}


void aspace_free(address_space_t* aspace) {
   
    DEBUG_ASSERT(aspace);

    if(--aspace->refcount > 0)
        return;


    list_each(aspace->mappings, map)
        aspace_munmap(aspace, map);

}

void aspace_destroy(address_space_t** aspace) {

    DEBUG_ASSERT(aspace);
    DEBUG_ASSERT(*aspace);

    aspace_free(*aspace);

    kfree((*aspace));
    (*aspace) = NULL;
}


void* aspace_mmap(address_space_t* aspace, void* address, size_t size, int flags) {

    DEBUG_ASSERT(aspace);
    DEBUG_ASSERT(size);


#if defined(DEBUG)

    char s[64] = { 0 };

    if(flags & ARCH_MAP_FIXED)
        strcat(s, "fixed,");
    if(flags & ARCH_MAP_NOEXEC)
        strcat(s, "noexec,");
    if(flags & ARCH_MAP_RDWR)
        strcat(s, "rw,");
    if(flags & ARCH_MAP_SHARED)
        strcat(s, "shared,");
    if(flags & ARCH_MAP_UNCACHED)
        strcat(s, "uncached,");
    if(flags & ARCH_MAP_USER)
        strcat(s, "user,");
    
    s[strlen(s) - 1] = '\0';


    kprintf("aspace: mapped %d bytes at %p-%p (%s)\n", size, address, (long) address + size, s);

#endif


    void* p;
    if((p = arch_mmap(aspace, address, size, flags)) == NULL)
        kpanic("aspace: FAIL! arch_mmap(%p, %p, %d, %p) return NULL address", aspace->vmmpd, address, size, flags);


    address_space_map_t* map = (address_space_map_t*) kcalloc(sizeof(address_space_map_t), 1, GFP_KERNEL);

    map->start = (uintptr_t) p;
    map->end   = (uintptr_t) p + size;
    map->flags = (uintptr_t) flags;

    list_push(aspace->mappings, map);


    return p;
}


void aspace_munmap(address_space_t* aspace, address_space_map_t* map) {

    DEBUG_ASSERT(aspace);
    DEBUG_ASSERT(map);
    DEBUG_ASSERT(map->start);
    DEBUG_ASSERT(map->end);
    DEBUG_ASSERT(map->start < map->end);


    arch_munmap(aspace, (void*) map->start, map->end - map->start);


    list_remove(aspace->mappings, map);
    kfree(map);

}