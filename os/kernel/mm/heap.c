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
#include <stdint.h>
#include <string.h>

static uintptr_t heap_base = CONFIG_HEAP_BASE;


static inline uintptr_t __heap_base_alloc(size_t size) {
    DEBUG_ASSERT(heap_base + size < CONFIG_HEAP_BASE + CONFIG_HEAP_SIZE);
            
    size += PAGE_SIZE;
    size &= ~(PAGE_SIZE - 1);
    

    uintptr_t p = heap_base;
    heap_base += size;
    heap_base += PAGE_SIZE;

    arch_mmap ((void*) p, size, ARCH_MAP_RDWR);
    return p;
}


int heap_create(heap_t** heap, size_t nsize, size_t nmax) {
    DEBUG_ASSERT(heap);
    DEBUG_ASSERT(nsize);
    DEBUG_ASSERT(nmax);
    DEBUG_ASSERT(!(nmax % 32));

    size_t size = sizeof(heap_t) + (nmax / 32) + (nmax * nsize);

    kprintf("heap: allocating %d KB at %p\n", size / 1024, heap_base);


    heap_t* h = (heap_t*) __heap_base_alloc(size);
    spinlock_init(&h->lock);

    h->nsize = nsize;
    h->nmax = nmax;
    h->nused = 0;
    h->data = (uintptr_t) &h->bitmap + (nmax / 32);
    h->end = (uintptr_t) h + size;

    memset(&h->bitmap, 0, (nmax / 32));


    *heap = h;
    return 0;
}


int heap_destroy(heap_t* heap) {
    DEBUG_ASSERT(heap);
    DEBUG_ASSERT(0 && "TODO: Dynamic Heap allocator");

    __lock(&heap->lock, {

        arch_munmap ((void*) heap, heap->end);

        pmm_unclaim((uintptr_t) heap, heap->end);
    
    });

    return 0;
}


void* heap_alloc(heap_t* heap) {
    DEBUG_ASSERT(heap);

    block_t block = -1;

    if(heap->nused > heap->nmax - 1)
        return NULL;


    __lock(&heap->lock, {
    
        int i;
        for(i = 0; (i < (heap->nmax / 32)) && (block == -1); i++) {
            if(unlikely(heap->bitmap[i] == 0xFFFFFFFF))
                continue;


            int m = heap->bitmap[i];
            int j = 0;

            for(; j < 32; j++) {
                if(m & (1 << j))
                    continue;

                heap->bitmap[i] |= (1 << j);
                block = (block_t) (i * 32) + j;
                break;
            }
        }

    });

    if(unlikely(block == -1))
        return NULL;

    heap->nused++;
    return (void*) (heap->data + (block * heap->nsize));
}


void heap_free(heap_t* heap, void* address) {
    DEBUG_ASSERT(heap);
    DEBUG_ASSERT(address);


    uintptr_t p = (uintptr_t) address;
    p -= heap->data;
    p /= heap->nsize;


    __lock(&heap->lock, {
        heap->bitmap[p / 32] &= ~(1 << (p % 32));
    });

    heap->nused--;
}