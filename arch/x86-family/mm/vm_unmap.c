/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
 *                                                                      
 * This file is part of aplus.                                          
 *                                                                      
 * aplus is free software: you can redistribute it and/or modify        
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or    
 * (at your option) any later version.                                  
 *                                                                      
 * aplus is distributed in the hope that it will be useful,             
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        
 * GNU General Public License for more details.                         
 *                                                                      
 * You should have received a copy of the GNU General Public License    
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.       
 */                                                                     
                                                                        
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/ipc.h>
#include <aplus/hal.h>

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/vmm.h>
#include <arch/x86/intr.h>




/*!
 * @brief arch_vmm_unmap().
 *        Unmap virtual memory.
 * 
 * @param space: address space.
 * @param virtaddr: virtual address.
 * @param length: size of virtual space.
 */
uintptr_t arch_vmm_unmap(vmm_address_space_t* space, uintptr_t virtaddr, size_t length) {

    DEBUG_ASSERT(space);
    DEBUG_ASSERT(length);


    uintptr_t pagesize = X86_MMU_PAGESIZE;

    uintptr_t s = virtaddr;
    uintptr_t e = virtaddr + length;


    if(s & (X86_MMU_PAGESIZE - 1))
        s = (s & ~(X86_MMU_PAGESIZE - 1));

    if(e & (X86_MMU_PAGESIZE - 1))
        e = (e & ~(X86_MMU_PAGESIZE - 1)) + X86_MMU_PAGESIZE;



    spinlock_lock(&space->lock);

    for(; s < e; s += X86_MMU_PAGESIZE) {

        x86_page_t* d;


#if defined(__x86_64__)

        /* CR3-L4 */ 
        {
            d = &((x86_page_t*) arch_vmm_p2v(space->pm, ARCH_VMM_AREA_HEAP)) [(s >> 39) & 0x1FF];
        }

        /* PML4-L3 */
        {
            DEBUG_ASSERT((*d != X86_MMU_CLEAR) && "PML4-L3 not exist");

            d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 30) & 0x1FF];
        }


        /* HUGE_1GB */
        if(!(*d & X86_MMU_PG_PS)) {

            /* PDP-L2 */
            {
                DEBUG_ASSERT((*d != X86_MMU_CLEAR) && "PDP-L2 not exist");

                d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 21) & 0x1FF];
            }

            /* HUGE_2MB */
            if(!(*d & X86_MMU_PG_PS)) {

                /* PD-L1 */
                {
                    DEBUG_ASSERT((*d != X86_MMU_CLEAR) && "PDT-L1 not exist");

                    d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 12) & 0x1FF];
                }

            } else
                pagesize = X86_MMU_HUGE_2MB_PAGESIZE;

        } else
            pagesize = X86_MMU_HUGE_1GB_PAGESIZE;


#elif defined(__i386__)

        /* CR3-L2 */
        {
            d = &((x86_page_t*) arch_vmm_p2v(space->pm, ARCH_VMM_AREA_HEAP)) [(s >> 22) & 0x3FF];
        }


        /* HUGE_4MB */
        if(!(*d & X86_MMU_PG_PS)) {

            /* PD-L1 */
            {
                DEBUG_ASSERT((*d != X86_MMU_CLEAR) && "PDT-L1 not exist");

                d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 12) & 0x3FF];
            }

        } else
            pagesize = X86_MMU_HUGE_2MB_PAGESIZE;

#endif

        /* Page Table */
        {
            DEBUG_ASSERT((*d != X86_MMU_CLEAR) && "Page already unmapped");

            if(*d & X86_MMU_PG_AP_PFB)
                pmm_free_blocks(*d & X86_MMU_ADDRESS_MASK, pagesize >> 12);


#if DEBUG_LEVEL_TRACE
            //kprintf("arch_vmm_unmap(): virtaddr(%p) physaddr(%p) pagesize(%p)\n", s, *d & X86_MMU_ADDRESS_MASK, pagesize);
#endif

            *d = X86_MMU_CLEAR;
    
        }


        __asm__ __volatile__ ("invlpg (%0)" :: "r"(virtaddr) : "memory");

        space->size -= pagesize >> 12;

    }

    spinlock_unlock(&space->lock);

    return virtaddr;

}

