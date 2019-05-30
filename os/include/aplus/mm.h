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


#ifndef _APLUS_MM_H
#define _APLUS_MM_H

#if defined(KERNEL)
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define ARCH_MAP_FIXED                          1
#define ARCH_MAP_USER                           2
#define ARCH_MAP_UNCACHED                       4
#define ARCH_MAP_RDWR                           8
#define ARCH_MAP_NOEXEC                         16
#define ARCH_MAP_SHARED                         32

#define MM_INIT_PMM                             1
#define MM_INIT_VMM                             2
#define MM_INIT_SLAB                            4

#define GFP_KERNEL                              1
#define GFP_USER                                2
#define GFP_ATOMIC                              3


typedef long block_t;


void* arch_mmap(void*, size_t, int);
void arch_munmap(void*, size_t);
int arch_ptr_access(void*, int);


void mm_init(int);
void slab_init(void);


void* kmalloc(size_t, int);
void* krealloc(void*, size_t, int);
void* kcalloc(size_t, size_t, int);
void kfree(void*);


void pmm_init(void);
void pmm_claim(uintptr_t, uintptr_t);
void pmm_unclaim(uintptr_t, uintptr_t);

block_t pmm_alloc_block(void);
block_t pmm_alloc_block_from_end(void);
void pmm_free_block(block_t);


#define pmm_alloc_block_safe() ({                                                   \
    block_t b = pmm_alloc_block();                                                  \
    DEBUG_ASSERT(b != -1);                                                          \
    memset((void*) ((uintptr_t)((b << 12) + CONFIG_KERNEL_BASE)), 0, PAGE_SIZE);    \
    b;                                                                              \
})

#define ptr_check(p, m) \
    arch_ptr_access((void*) p, m)


#endif
#endif