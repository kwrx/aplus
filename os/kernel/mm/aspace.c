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
    DEBUG_ASSERT(start <= end);
    DEBUG_ASSERT((*aspace) == NULL);


    address_space_t* s = (*aspace) = (address_space_t*) kcalloc(sizeof(address_space_t), 1, GFP_KERNEL);

    s->start = start;
    s->end = end;
    s->size = 0;
    s->vmmpd = vmmpd;

    s->ops.open   = ops->open;
    s->ops.close  = ops->close;
    s->ops.nopage = ops->nopage;
    
}


void aspace_create_nomem(address_space_t** aspace, address_space_t* staticbuf, uintptr_t vmmpd, address_space_ops_t* ops, uintptr_t start, uintptr_t end) {

    DEBUG_ASSERT(aspace);
    DEBUG_ASSERT(ops);
    DEBUG_ASSERT(start <= end);
    DEBUG_ASSERT((*aspace) == NULL);


    address_space_t* s = (*aspace) = staticbuf;

    s->start = start;
    s->end = end;
    s->size = 0;
    s->vmmpd = vmmpd;

    s->ops.open   = ops->open;
    s->ops.close  = ops->close;
    s->ops.nopage = ops->nopage;
    
}


void aspace_clone(address_space_t** dest, address_space_t* src) {
    
    DEBUG_ASSERT(src);
    DEBUG_ASSERT(dest);
    DEBUG_ASSERT((*dest) == NULL);


    aspace_create(dest, src->vmmpd, &src->ops, src->start, src->end);

    list_each(src->mappings, map)
        aspace_ref_map(*dest, map);

}


void aspace_free(address_space_t* aspace) {
   
    DEBUG_ASSERT(aspace);


    list_each(aspace->mappings, map)
        aspace_remove_map(aspace, map);

}

void aspace_destroy(address_space_t** aspace) {

    DEBUG_ASSERT(aspace);
    DEBUG_ASSERT(*aspace);

    aspace_free(*aspace);

    kfree((*aspace));
    (*aspace) = NULL;
}


 

void aspace_enlarge_map(address_space_t* aspace, address_space_map_t* map, size_t incr) {

    DEBUG_ASSERT(aspace);
    DEBUG_ASSERT(map);
    DEBUG_ASSERT(incr);


    if(incr & (PAGE_SIZE - 1))
        incr = (incr & ~(PAGE_SIZE - 1)) + PAGE_SIZE;



    if(map->end + incr > aspace->end)
        kpanic("aspace: FAIL! Address Space limit reached on enlarge! type(%d), end(%p), limit(%p)", map->type, map->end + incr, aspace->end);


    __lock(&map->lock, {
        
        if(arch_mmap(aspace, (void*) map->end, incr - 1, map->flags) == NULL)
            kpanic("aspace: FAIL! arch_mmap(%p, %p, %d, %p) return NULL address", aspace->vmmpd, map->end, incr, map->flags);

        map->end += incr;

    });
}


void aspace_shrink_map(address_space_t* aspace, address_space_map_t* map, size_t decr) {
    
    DEBUG_ASSERT(aspace);
    DEBUG_ASSERT(map);
    DEBUG_ASSERT(decr);


    if(map->end - decr < aspace->start)
        kpanic("aspace: FAIL! Address Space limit reached on shrink! type(%d), end(%p), limit(%p)", map->type, map->end - decr, aspace->start);


    __lock(&map->lock, {

        arch_munmap(aspace, (void*) (map->end - decr), decr - 1);

        map->end -= decr;

    });

}


void aspace_add_map(address_space_t* aspace, uint8_t type, uintptr_t start, uintptr_t end, uint32_t flags) {

    DEBUG_ASSERT(aspace);
    DEBUG_ASSERT(type != ASPACE_TYPE_UNKNOWN);
    DEBUG_ASSERT(start <= end);


    address_space_map_t* map = (address_space_map_t*) kcalloc(sizeof(address_space_map_t), 1, GFP_KERNEL);

    map->type  = (uintptr_t) type;
    map->start = (uintptr_t) start;
    map->end   = (uintptr_t) start;
    map->flags = (uintptr_t) flags;
    map->refcount = 1;

    spinlock_init(&map->lock);


    switch(type) {
        
        case ASPACE_TYPE_TLS:
        case ASPACE_TYPE_SWAP:

            map->end = end;
            break;
        
        default:
            if(end > start)
                aspace_enlarge_map(aspace, map, end - start);

            break;
    
    }


#if defined(DEBUG)

    char s[64] = { 0 };

    switch(type) {
        case ASPACE_TYPE_CODE:   strcat(s, "code,");   break;
        case ASPACE_TYPE_DATA:   strcat(s, "data,");   break;
        case ASPACE_TYPE_TLS:    strcat(s, "tls,");    break;
        case ASPACE_TYPE_BRK:    strcat(s, "brk,");    break;
        case ASPACE_TYPE_SHARED: strcat(s, "shared,"); break;
        case ASPACE_TYPE_SWAP:   strcat(s, "swap,");   break;
    }

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


    kprintf("aspace: mapped %d bytes at %p-%p (%s)\n", map->end - map->start, map->start, map->end, s);

#endif

    list_push(aspace->mappings, map);

}



void aspace_ref_map(address_space_t* aspace, address_space_map_t* map) {

    DEBUG_ASSERT(aspace);
    DEBUG_ASSERT(map);


    list_push(aspace->mappings, ptr_ref(map));

}



void aspace_remove_map(address_space_t* aspace, address_space_map_t* map) {

    DEBUG_ASSERT(aspace);
    DEBUG_ASSERT(map);
    DEBUG_ASSERT(map->start);
    DEBUG_ASSERT(map->end);


    list_remove(aspace->mappings, map);


    if(--map->refcount > 0)
        return;

    if(map->end > map->start)
        arch_munmap(aspace, (void*) map->start, map->end - map->start);

    kfree(map);

}


void aspace_get_maps(address_space_t* aspace, address_space_map_t** maps, uint8_t type, size_t size) {

    DEBUG_ASSERT(aspace);

    int i = 0;
    list_each(aspace->mappings, map) {
        
        if(i == size)
            break;

        if(map->type == type)
            maps[i++] = map;

    }

}