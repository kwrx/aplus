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
 * @brief arch_vmm_map().
 *        Map virtual memory.
 * 
 * @param space: address space.
 * @param virtaddr: virtual address.
 * @param physaddr: physical address.
 * @param length: size of virtual space.
 * @param flags: @see include/arch/x86/vmm.h
 */
uintptr_t arch_vmm_map(vmm_address_space_t* space, uintptr_t virtaddr, uintptr_t physaddr, size_t length, int flags) {

    DEBUG_ASSERT(space);
    DEBUG_ASSERT(length);


    uintptr_t pagesize;

    uintptr_t s = virtaddr;
    uintptr_t p = physaddr;
    uintptr_t e = virtaddr + length;


    if(flags & ARCH_VMM_MAP_HUGETLB) {

#if defined(__x86_64__)
        if(flags & ARCH_VMM_MAP_HUGE_1GB)
            pagesize = X86_MMU_HUGE_1GB_PAGESIZE;
        else
#endif
            pagesize = X86_MMU_HUGE_2MB_PAGESIZE;
    
    } else
        pagesize = X86_MMU_PAGESIZE; 




    if(s & (pagesize - 1))
        s = (s & ~(pagesize - 1));

    if(p & (pagesize - 1))
        p = (p & ~(pagesize - 1));

    if(e & (pagesize - 1))
        e = (e & ~(pagesize - 1)) + pagesize;





    uint64_t b = X86_MMU_PG_P;      // flags for Page Table
    uint64_t q = X86_MMU_PG_P;      // flags for Page Directory



    // * Prepare FLAGS for the Page Directory

#if defined(__x86_64__)

    if(virtaddr < 800000000000)     // Low Address (userspace)
        q |= X86_MMU_PG_U;

#elif defined(__i386__)

    if(virtaddr < 0xC0000000)       // Low Address (userspace)
        q |= X86_MMU_PG_U;

#endif

    q |= X86_MMU_PG_RW;




    // * Prepare FLAGS for the Page Table

    if(flags & ARCH_VMM_MAP_DISABLED)
        b &= ~X86_MMU_PG_P;

    if(flags & ARCH_VMM_MAP_RDWR)
        b |= X86_MMU_PG_RW;

    if(flags & ARCH_VMM_MAP_USER)
        b |= X86_MMU_PG_U;

    if(flags & ARCH_VMM_MAP_UNCACHED)
        b |= X86_MMU_PG_CD;

    if(flags & ARCH_VMM_MAP_SHARED)
        b |= X86_MMU_PG_G;



#if DEBUG_LEVEL_TRACE
    if(flags & ARCH_VMM_MAP_DEMAND)
        DEBUG_ASSERT((flags & ARCH_VMM_MAP_TYPE_MASK) == ARCH_VMM_MAP_TYPE_PAGE && "Only TYPE_PAGE can be no-prefault");
#endif

    //* Set Page Type
    switch((flags & ARCH_VMM_MAP_TYPE_MASK)) {

        case ARCH_VMM_MAP_TYPE_PAGE:
            b |= X86_MMU_PG_AP_TP_PAGE;
            break;

        case ARCH_VMM_MAP_TYPE_MMAP:
            b |= X86_MMU_PG_AP_TP_MMAP;
            break;

        case ARCH_VMM_MAP_TYPE_COW:
            b |= X86_MMU_PG_AP_TP_COW;
            break;

    }


#if defined(__x86_64__)

    //* Set No-Execute Bit
    if(flags & ARCH_VMM_MAP_NOEXEC)
        if(boot_cpu_has(X86_FEATURE_NX))
            b |= X86_MMU_PT_NX;             /* NX */

#endif

    if(flags & ARCH_VMM_MAP_HUGETLB) {

        b |= X86_MMU_PG_PS;

        if(flags & ARCH_VMM_MAP_VIDEO_MEMORY)  
            if(boot_cpu_has(X86_FEATURE_PAT))
                b |= X86_MMU_PG_PAT;        /* WC */

    } else {

        if(flags & ARCH_VMM_MAP_VIDEO_MEMORY)  
            if(boot_cpu_has(X86_FEATURE_PAT))
                b |= X86_MMU_PT_PAT;        /* WC */

    }






    spinlock_lock(&space->lock);

    for(; s < e; s += pagesize, p += pagesize) {

        x86_page_t* d;


#if defined(__x86_64__)

        /* CR3-L4 */
        {
            d = &((x86_page_t*) arch_vmm_p2v(space->pm, ARCH_VMM_AREA_HEAP)) [(s >> 39) & 0x1FF];
        }

        /* PML4-L3 */
        {
            if(*d == X86_MMU_CLEAR)
                *d = __alloc_page(X86_MMU_PAGESIZE, 1) | q;

            d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 30) & 0x1FF];
        }


        if(pagesize != X86_MMU_HUGE_1GB_PAGESIZE) {

            /* PDP-L2 */
            {
                DEBUG_ASSERT(!(*d & X86_MMU_PG_PS) && "PDP-L2 is 1GiB Page");

                if(*d == X86_MMU_CLEAR)
                    *d = __alloc_page(X86_MMU_PAGESIZE, 1) | q;

                d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 21) & 0x1FF];

            }

            if(pagesize != X86_MMU_HUGE_2MB_PAGESIZE) {

                /* PD-L1 */
                {
                    DEBUG_ASSERT(!(*d & X86_MMU_PG_PS) && "PD-L1 is 2Mib Page");

                    if(*d == X86_MMU_CLEAR)
                        *d = __alloc_page(X86_MMU_PAGESIZE, 1) | q;

                    d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 12) & 0x1FF];
                }

            }

        }

#elif defined(__i386__)

        /* CR3-L2 */
        {
            d = &((x86_page_t*) arch_vmm_p2v(space->pm, ARCH_VMM_AREA_HEAP)) [(s >> 22) & 0x3FF];
        }


        if(pagesize != X86_MMU_HUGE_2MB_PAGESIZE) {

            /* PD-L1 */
            {
                DEBUG_ASSERT(!(*d & X86_MMU_PG_PS) && "PD-L1 is 4Mib Page");

                if(*d == X86_MMU_CLEAR)
                    *d = __alloc_page(X86_MMU_PAGESIZE, 1) | q;

                d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 12) & 0x3FF];
            }

        }

#endif

        /* Page Table */
        {
            // TODO: add support for releasing pages when process is killed
            // DEBUG_ASSERT((*d == X86_MMU_CLEAR) && "Page already used, unmap first");


            if(flags & ARCH_VMM_MAP_FIXED) {
              
                *d = p | b;

            } else {

                if(flags & ARCH_VMM_MAP_DEMAND)
                    *d = X86_MMU_PG_AP_TP_COW | (b & ~X86_MMU_PG_P);
                else
                    *d = __alloc_page(pagesize, 0) | X86_MMU_PG_AP_PFB | b;

            }


#if DEBUG_LEVEL_TRACE
            // // kprintf("arch_vmm_map(): virtaddr(0x%lX) physaddr(0x%llX) flags(0x%lX) pagesize(0x%lX)\n", s, *d & X86_MMU_ADDRESS_MASK, b, pagesize);
#endif

        }


        __asm__ __volatile__ ("invlpg (%0)" :: "r"(s) : "memory");
        
        space->size += pagesize >> 12;

    }

    spinlock_unlock(&space->lock);

    return virtaddr;

}
