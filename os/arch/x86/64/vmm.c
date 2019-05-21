
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
#include <aplus/mm.h>
#include <aplus/ipc.h>
#include <arch/x86/mm.h>
#include <stdint.h>
#include <string.h>


static inline block_t alloc_page() {
    block_t p = pmm_alloc_block_safe();

    memset((void*) ((p << 12) + CONFIG_KERNEL_BASE), 0, PAGE_SIZE);
    return p;
}


void x86_map_page(x86_page_t* aspace, uintptr_t address, block_t block, uint64_t flags) {
    DEBUG_ASSERT(aspace != NULL);
    DEBUG_ASSERT((void*) aspace > (void*) CONFIG_KERNEL_BASE);    


    x86_page_t* d;

    /* CR3-L4 */
    {
        d = &aspace[(address >> 39) & 0x1FF];
    }

    /* PML4-L3 */
    {
        if(!(*d & X86_MMU_PG_P))
            *d = ((uint64_t) alloc_page() << 12) | X86_MMU_PG_P;

        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 30) & 0x1FF];
    }

    /* PDP-L2 */
    {
        DEBUG_ASSERT(!(*d & X86_MMU_PG_PS) && "PDP-L2 is 1GiB Page");

        if(!(*d & X86_MMU_PG_P))
            *d = ((uint64_t) alloc_page() << 12) | X86_MMU_PG_P;

        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 21) & 0x1FF];
    }

    /* PD-L1 */
    {
        DEBUG_ASSERT(!(*d & X86_MMU_PG_PS) && "PD-L1 is 2Mib Page");

        if(!(*d & X86_MMU_PG_P))
            *d = ((uint64_t) alloc_page() << 12) | flags;

        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 12) & 0x1FF];
    }

    /* Page Table */
    {
        DEBUG_ASSERT(!(*d & X86_MMU_PG_P) && "Page already used, unmap first");

        if(block == -1)
            *d = ((uint64_t) alloc_page() << 12) | X86_MMU_PG_AP_PFB | flags;
        else
            *d = ((uint64_t) block << 12) | flags;
    }


    __asm__ __volatile__ ("invlpg [%0]" :: "m"(address) : "memory");
}

void x86_unmap_page(x86_page_t* aspace, uintptr_t address) {
    DEBUG_ASSERT(aspace != NULL);
    DEBUG_ASSERT((void*) aspace > (void*) CONFIG_KERNEL_BASE);    


    x86_page_t* d;

    /* CR3-L4 */ 
    {
        d = &aspace[(address >> 39) & 0x1FF];
    }

    /* PML4-L3 */
    {
        DEBUG_ASSERT((*d & X86_MMU_PG_P) && "PML4-L3 not exist");

        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 30) & 0x1FF];
    }

    /* PDP-L2 */
    {
        DEBUG_ASSERT((*d & X86_MMU_PG_P) && "PDP-L2 not exist");

        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 21) & 0x1FF];
    }

    /* PD-L1 */
    {
        DEBUG_ASSERT((*d & X86_MMU_PG_P) && "PDT-L1 not exist");

        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 12) & 0x1FF];
    }

    /* Page Table */
    {
        DEBUG_ASSERT((*d & X86_MMU_PG_P) && "Page already unmapped");

        if(*d & X86_MMU_PG_AP_PFB)
            pmm_free_block((*d >> 12));

        *d = X86_MMU_CLEAR;
    }


    __asm__ __volatile__ ("invlpg [%0]" :: "m"(address) : "memory");
}