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

spinlock_t memlock;


#define HAVE_MORECORE                       1
#define HAVE_MMAP                           0
#define HAVE_MREMAP                         0

#define USE_DL_PREFIX                       1
#define USE_BUILTIN_FFS                     0

#define MORECORE_CANNOT_TRIM                1
#define ABORT_ON_ASSERT_FAILURE             1
#define NO_MALLOC_STATS                     1
#define REALLOC_ZERO_BYTES_FREES            1
#define MMAP_CLEARS                         0
#define MALLOC_ALIGNMENT                    16


#define malloc_getpagesize                  \
    ((size_t) PAGE_SIZE)

#define DLMALLOC_EXPORT                     \
    static

#define MORECORE                            \
    __morecore

#define ABORT                               \
    kpanic("dlmalloc: abort() at %s:%d!", __FILE__, __LINE__)

#define MALLOC_FAILURE_ACTION               \
    kpanic("dlmalloc: nomem() at %s:%d", __FILE__, __LINE__)



static void* __morecore(intptr_t incr) {
    static uintptr_t base = CONFIG_HEAP_BASE;
    uintptr_t ptr = base;

    DEBUG_ASSERT(incr >= 0);
    DEBUG_ASSERT(base <= CONFIG_HEAP_BASE + CONFIG_HEAP_SIZE);

    base += incr;
    return (void*) ptr;
}


#include "dlmalloc.h"


void slab_init(void) {
    spinlock_init(&memlock);
}

void* kmalloc(size_t size, int gfp) {
    (void) gfp;

    __lock_return(&memlock, void*, dlmalloc(size));
}

void* krealloc(void* ptr, size_t size, int gfp) {
    (void) gfp;

    __lock_return(&memlock, void*, dlrealloc(ptr, size));
}

void* kcalloc(size_t nm, size_t cc, int gfp) {
    (void) gfp;

    __lock_return(&memlock, void*, dlcalloc(nm, cc));
}

void kfree(void* ptr) {
    __lock(&memlock, dlfree(ptr));
}

