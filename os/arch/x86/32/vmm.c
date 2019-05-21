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
#include <arch/x86/cpu.h>
#include <stdint.h>
#include <unistd.h>




void x86_map_page(x86_page_t* aspace, uintptr_t address, block_t block, uint64_t flags) {
    DEBUG_ASSERT(aspace != NULL);
    DEBUG_ASSERT((void*) aspace > (void*) CONFIG_KERNEL_BASE);    


    x86_page_t* d;

    /* CR3-L2 */ 
    {
        d = &aspace[(address >> 22) & 0x3FF];
    }

    /* PD-L1 */
    {
        DEBUG_ASSERT(!(*d & X86_MMU_PG_PS) && "PD-L1 is 4Mib Page");

        if(!(*d & X86_MMU_PG_P))
            *d = (pmm_alloc_block_safe() << 12) | flags;

        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 12) & 0x3FF];
    }

    /* Page Table */
    {
        DEBUG_ASSERT(!(*d & X86_MMU_PG_P) && "Page already used, unmap first");

        if(block == -1)
            *d = (pmm_alloc_block_safe() << 12) | X86_MMU_PG_AP_PFB | flags;
        else
            *d = (block << 12) | flags;
    }


    __asm__ __volatile__ ("invlpg [%0]" :: "m"(address) : "memory");
}

void x86_unmap_page(x86_page_t* aspace, uintptr_t address) {
    DEBUG_ASSERT(aspace != NULL);
    DEBUG_ASSERT((void*) aspace > (void*) CONFIG_KERNEL_BASE);    


    x86_page_t* d;

    /* CR3-L2 */ 
    {
        d = &aspace[(address >> 22) & 0x3FF];
    }

    /* PD-L1 */
    {
        DEBUG_ASSERT((*d & X86_MMU_PG_P) && "PDT-L1 not exist");

        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 12) & 0x3FF];
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

int x86_ptr_access(uintptr_t address, int mode) {

    x86_page_t* d;
    x86_page_t* aspace = (x86_page_t*) (CONFIG_KERNEL_BASE + x86_get_cr3());
    
    /* CR3-L2 */ 
    {
        d = &aspace[(address >> 22) & 0x3FF];
    }

    /* PD-L1 */
    {
        if(unlikely(!(*d & X86_MMU_PG_P)))
            return 0;

        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 12) & 0x3FF];
    }

    /* Page Table */
    
    if(mode & R_OK)
        if(unlikely(!(*d & X86_MMU_PG_P)))
            return 0;

    if(mode & W_OK)
        if(unlikely(!(*d & X86_MMU_PG_RW)))
            return 0;

    return 1;
}