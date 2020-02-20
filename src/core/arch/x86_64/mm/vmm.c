/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
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
                                                                        
#include <stdint.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>
#include <aplus/core/endian.h>

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>


#if 0
void x86_map_page(x86_pt_t* d, uintptr_t virtaddr, uintptr_t physaddr, uint64_t flags) {

    DEBUG_ASSERT(d != NULL);
    DEBUG_ASSERT((virtaddr & (X86_MMU_PAGESIZE - 1)) == 0);
    DEBUG_ASSERT((physaddr & (X86_MMU_PAGESIZE - 1)) == 0);


    //! Disable NX bit on High Address
    if(physaddr & (1ULL << 63))
        flags &= ~X86_MMU_PG_NX;


    /* CR3-L4 */
    {
        d = &d[(virtaddr >> 39) & 0x1FF];
    }

    /* PML4-L3 */
    {
        if(!(*d & X86_MMU_PG_P))
            *d = pmm_alloc_block_safe() | X86_MMU_PG_P;

        d = &((x86_pt_t*) arch_vmm_p2v(*d & ~0xFFF, ARCH_VMM_AREA_HEAP)) [(virtaddr >> 30) & 0x1FF];
    }

    /* PDP-L2 */
    {
        DEBUG_ASSERT(!(*d & X86_MMU_PG_PS) && "PDP-L2 is 1GiB Page");

        if(!(*d & X86_MMU_PG_P))
            *d = pmm_alloc_block_safe() | X86_MMU_PG_P;

        d = &((x86_pt_t*) arch_vmm_p2v(*d & ~0xFFF, ARCH_VMM_AREA_HEAP)) [(virtaddr >> 21) & 0x1FF];
    }

    /* PD-L1 */
    {
        DEBUG_ASSERT(!(*d & X86_MMU_PG_PS) && "PD-L1 is 2Mib Page");

        if(!(*d & X86_MMU_PG_P))
            *d = pmm_alloc_block_safe() | flags;

        d = &((x86_pt_t*) arch_vmm_p2v(*d & ~0xFFF, ARCH_VMM_AREA_HEAP)) [(virtaddr >> 12) & 0x1FF];
    }

    /* Page Table */
    {
        DEBUG_ASSERT(!(*d & X86_MMU_PG_P) && "Page already used, unmap first");

        if(flags & X86_MMU_PG_AP_PAT) /* WC */
            flags |= X86_MMU_PT_PAT;


        if(physaddr == -1)
            *d = pmm_alloc_block_safe() | X86_MMU_PG_AP_PFB | flags;
        else
            *d = physaddr | flags;
    }


    __asm__ __volatile__ ("invlpg [%0]" :: "m"(virtaddr) : "memory");

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


int x86_ptr_access(uintptr_t address, int mode) {

    x86_page_t* d;
    x86_page_t* aspace = (x86_page_t*) (CONFIG_KERNEL_BASE + x86_get_cr3());

    /* CR3-L4 */ 
    {
        d = &aspace[(address >> 39) & 0x1FF];
    }

    /* PML4-L3 */
    {
        if(unlikely(!(*d & X86_MMU_PG_P)))
            return 0;

        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 30) & 0x1FF];
    }

    /* PDP-L2 */
    {
        if(unlikely(!(*d & X86_MMU_PG_P)))
            return 0;

        if(unlikely(*d & X86_MMU_PG_PS)) /* Check 1GiB Page */
            goto check;
            
        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 21) & 0x1FF];
    }

    /* PD-L1 */
    {
        if(unlikely(!(*d & X86_MMU_PG_P)))
            return 0;

        if(unlikely(*d & X86_MMU_PG_PS)) /* Check 2MiB Page */
            goto check;

        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 12) & 0x1FF];
    }

    /* Page Table */
check:
    if(mode & R_OK)
        if(unlikely(!(*d & X86_MMU_PG_P)))
            return 0;

    if(mode & W_OK)
        if(unlikely(!(*d & X86_MMU_PG_RW)))
            return 0;

    return 1;
}


uintptr_t x86_ptr_phys(uintptr_t address) {

    x86_page_t* d;
    x86_page_t* aspace = (x86_page_t*) (CONFIG_KERNEL_BASE + x86_get_cr3());

    uintptr_t addend = address & (PAGE_SIZE - 1);
    

    /* CR3-L4 */ 
    {
        d = &aspace[(address >> 39) & 0x1FF];
    }

    /* PML4-L3 */
    {
        if(unlikely(!(*d & X86_MMU_PG_P)))
            return 0;

        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 30) & 0x1FF];
    }

    /* PDP-L2 */
    {
        if(unlikely(!(*d & X86_MMU_PG_P)))
            return 0;

        if(unlikely(*d & X86_MMU_PG_PS)) /* Check 1GiB Page */
            { addend = address & 0x3FFFFFFF; goto check; }
            
        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 21) & 0x1FF];
    }

    /* PD-L1 */
    {
        if(unlikely(!(*d & X86_MMU_PG_P)))
            return 0;

        if(unlikely(*d & X86_MMU_PG_PS)) /* Check 2MiB Page */
            { addend = address & 0x1FFFFF; goto check; }

        d = &((x86_page_t*) ((uintptr_t) (*d & ~0xFFF) + CONFIG_KERNEL_BASE)) [(address >> 12) & 0x1FF];
    }

    /* Page Table */
check:    
    if(unlikely(!(*d & X86_MMU_PG_P)))
        return 0;

    return ((*d >> 12) << 12) + addend;
}
#endif