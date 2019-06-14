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
#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>
#include <arch/x86/mm.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>


void* arch_mmap(void* address, size_t length, int flags) {
    DEBUG_ASSERT(length);

    uintptr_t s = ((uintptr_t) address) & ~(PAGE_SIZE - 1);
    uintptr_t e = ((uintptr_t) address + length) & ~(PAGE_SIZE - 1);

    uint64_t mf = X86_MMU_PG_P;


    if(flags & ARCH_MAP_RDWR)
        mf |= X86_MMU_PG_RW;

    if(flags & ARCH_MAP_NOEXEC)
        mf |= X86_MMU_PG_NX;

    if(flags & ARCH_MAP_USER)
        mf |= X86_MMU_PG_U;

    if(flags & ARCH_MAP_UNCACHED)
        mf |= X86_MMU_PG_CD;

    if(flags & ARCH_MAP_SHARED)
        mf |= X86_MMU_PG_G;

    if(flags & ARCH_MAP_VIDEO_MEMORY)
        mf |= X86_MMU_PG_AP_PAT;



    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(current_task->aspace);
    

    __lock(&current_task->aspace->lock, {
    
        for(; s <= e; s += PAGE_SIZE)
            if(flags & ARCH_MAP_FIXED)
                x86_map_page((x86_page_t*) (current_task->aspace->vmmpd + CONFIG_KERNEL_BASE), s, s >> 12, mf);
            else
                x86_map_page((x86_page_t*) (current_task->aspace->vmmpd + CONFIG_KERNEL_BASE), s, -1, mf);
    
    });

    return address;
}

void arch_munmap(void* address, size_t length) {
    DEBUG_ASSERT(length);

    uintptr_t s = ((uintptr_t) address) & ~(PAGE_SIZE - 1);
    uintptr_t e = ((uintptr_t) address + length + PAGE_SIZE) & ~(PAGE_SIZE - 1);


    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(current_task->aspace);


    __lock(&current_task->aspace->lock, {

        for(; s < e; s += PAGE_SIZE)
            x86_unmap_page((x86_page_t*) (current_task->aspace->vmmpd + CONFIG_KERNEL_BASE), s);
    
    });
}

int arch_ptr_access(void* ptr, int mode) {
    DEBUG_ASSERT(ptr);
    
    return x86_ptr_access((uintptr_t) ptr, mode);
}

void* arch_ptr_phys(void* ptr) {
    DEBUG_ASSERT(ptr);

    return (void*) x86_ptr_phys((uintptr_t) ptr);
}