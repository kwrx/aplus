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

#define __init_ns "memory::"

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/smp.h>
#include <stdint.h>


void vmm_init(void) {


    arch_mmap (
        current_task->aspace,
        (void*)  CONFIG_HEAP_BASE,
        (size_t) CONFIG_HEAP_PREALLOC_SIZE - 1,
        
        ARCH_MAP_NOEXEC | ARCH_MAP_RDWR | ARCH_MAP_SHARED
    );


    kprintf("heap: initialized at %p-%p, pre-allocated: %p\n", CONFIG_HEAP_BASE, CONFIG_HEAP_BASE + CONFIG_HEAP_SIZE, CONFIG_HEAP_PREALLOC_SIZE);
}


void mm_init(int flags) {
    if(flags & MM_INIT_PMM)
        __init(pmm, ());

    if(flags & MM_INIT_VMM)
        __init(vmm, ());

    if(flags & MM_INIT_SLAB)
        __init(slab, ());
}