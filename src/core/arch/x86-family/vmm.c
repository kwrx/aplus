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
#include <string.h>
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/endian.h>
#include <aplus/memory.h>
#include <aplus/ipc.h>

#include <hal/cpu.h>
#include <hal/interrupt.h>
#include <hal/vmm.h>

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/vmm.h>
#include <arch/x86/intr.h>


static uintptr_t alloc_page(uintptr_t pagesize, int zero) {

    DEBUG_ASSERT(pagesize);
    DEBUG_ASSERT(X86_MMU_PAGESIZE == PML1_PAGESIZE);


    uintptr_t p;

    if(likely(pagesize == X86_MMU_PAGESIZE))
        p = pmm_alloc_block();
    else
        p = pmm_alloc_blocks_aligned(pagesize >> 12, pagesize);

    
    if(likely(zero))
        memset((void*) arch_vmm_p2v(p, ARCH_VMM_AREA_HEAP), 0, pagesize);

    return p;
}





/*!
 * @brief arch_vmm_getpagesize().
 *        Get page size.
 */
uintptr_t arch_vmm_getpagesize() {
    return X86_MMU_PAGESIZE;
}


/*!
 * @brief arch_vmm_gethugepagesize().
 *        Get huge page size.
 */
uintptr_t arch_vmm_gethugepagesize() {
    return X86_MMU_HUGE_2MB_PAGESIZE;
}



/*!
 * @brief arch_vmm_p2v().
 *        Convert a physical address to virtual one.
 * 
 * @param physaddr: physical address.
 * @param type: type of memory area.
 */
uintptr_t arch_vmm_p2v(uintptr_t physaddr, int type) {

    switch(type) {

        case ARCH_VMM_AREA_HEAP:
            return physaddr + KERNEL_HEAP_AREA;
        
        case ARCH_VMM_AREA_KERNEL:
            return physaddr + KERNEL_HIGH_AREA;

    }

    BUG_ON(0);
    return -1;

}


/*!
 * @brief arch_vmm_v2p().
 *        Convert a virtual address to physical one.
 * 
 * @param virtaddr: virtual address.
 * @param type: type of memory area.
 */
uintptr_t arch_vmm_v2p(uintptr_t virtaddr, int type) {

    switch(type) {

        case ARCH_VMM_AREA_HEAP:
            return virtaddr - KERNEL_HEAP_AREA;
        
        case ARCH_VMM_AREA_KERNEL:
            return virtaddr - KERNEL_HIGH_AREA;

        //case ARCH_VMM_AREA_USER:
            //return __x86_ptr_phys(virtaddr);

    }

    BUG_ON(0);
    return -1;

}


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





    uint64_t b = X86_MMU_PG_P;

    if(flags & ARCH_VMM_MAP_RDWR)
        b |= X86_MMU_PG_RW;

    if(flags & ARCH_VMM_MAP_USER)
        b |= X86_MMU_PG_U;

    if(flags & ARCH_VMM_MAP_UNCACHED)
        b |= X86_MMU_PG_CD;

    if(flags & ARCH_VMM_MAP_SHARED)
        b |= X86_MMU_PG_G;




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
                *d = alloc_page(X86_MMU_PAGESIZE, 1) | X86_MMU_PG_RW | X86_MMU_PG_P;

            d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 30) & 0x1FF];
        }


        if(pagesize != X86_MMU_HUGE_1GB_PAGESIZE) {

            /* PDP-L2 */
            {
                DEBUG_ASSERT(!(*d & X86_MMU_PG_PS) && "PDP-L2 is 1GiB Page");

                if(*d == X86_MMU_CLEAR)
                    *d = alloc_page(X86_MMU_PAGESIZE, 1) | X86_MMU_PG_RW | X86_MMU_PG_P;

                d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 21) & 0x1FF];

            }

            if(pagesize != X86_MMU_HUGE_2MB_PAGESIZE) {

                /* PD-L1 */
                {
                    DEBUG_ASSERT(!(*d & X86_MMU_PG_PS) && "PD-L1 is 2Mib Page");

                    if(*d == X86_MMU_CLEAR)
                        *d = alloc_page(X86_MMU_PAGESIZE, 1) | X86_MMU_PG_RW | X86_MMU_PG_P;

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
                    *d = alloc_page(X86_MMU_PAGESIZE, 1) | X86_MMU_PG_RW | X86_MMU_PG_P;

                d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 12) & 0x3FF];
            }

        }

#endif

        /* Page Table */
        {
            DEBUG_ASSERT((*d == X86_MMU_CLEAR) && "Page already used, unmap first");


#if defined(DEBUG)
            if(flags & ARCH_VMM_MAP_DEMAND)
                DEBUG_ASSERT((flags & ARCH_VMM_MAP_TYPE_MASK) == ARCH_VMM_MAP_TYPE_PAGE && "Only TYPE_PAGE can be no-prefault");
#endif

            //* Set Page Type
            switch((flags & ARCH_VMM_MAP_TYPE_MASK)) {

                case ARCH_VMM_MAP_TYPE_PAGE:
                    b |= X86_MMU_PG_AP_TP_PAGE;
                    break;

                case ARCH_VMM_MAP_TYPE_UNIQUE:
                    b |= X86_MMU_PG_AP_TP_UNIQUE;
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


            if(flags & ARCH_VMM_MAP_FIXED)
                *d = p | b;

            else {

                if(flags & ARCH_VMM_MAP_DEMAND)
                    *d = X86_MMU_PG_AP_TP_COW | (b & ~X86_MMU_PG_P);
                else
                    *d = alloc_page(pagesize, 0) | X86_MMU_PG_AP_PFB | b;

            }


#if defined(DEBUG) && DEBUG_LEVEL >= 4
            //kprintf("arch_vmm_map(): virtaddr(%p) physaddr(%p) flags(%p) pagesize(%p)\n", s, *d & X86_MMU_ADDRESS_MASK, b, pagesize);
#endif

        }


        __asm__ __volatile__ ("invlpg (%0)" :: "r"(s) : "memory");
        
        space->size += pagesize >> 12;

    }

    spinlock_unlock(&space->lock);

    return virtaddr;

}



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


#if defined(DEBUG) && DEBUG_LEVEL >= 4
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






/*!
 * @brief arch_vmm_mprotect().
 *        Protect virtual memory.
 * 
 * @param space: address space.
 * @param virtaddr: virtual address.
 * @param length: size of virtual space.
 * @param flags: @see include/arch/x86/vmm.h
 */
uintptr_t arch_vmm_mprotect(vmm_address_space_t* space, uintptr_t virtaddr, size_t length, int flags) {

    DEBUG_ASSERT(space);
    DEBUG_ASSERT(length);


    uintptr_t pagesize;

    uintptr_t s = virtaddr;
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

    if(e & (pagesize - 1))
        e = (e & ~(pagesize - 1)) + pagesize;




    uint64_t b = X86_MMU_PG_P;

    if(flags & ARCH_VMM_MAP_RDWR)
        b |= X86_MMU_PG_RW;

    if(flags & ARCH_VMM_MAP_USER)
        b |= X86_MMU_PG_U;

    if(flags & ARCH_VMM_MAP_UNCACHED)
        b |= X86_MMU_PG_CD;

    if(flags & ARCH_VMM_MAP_SHARED)
        b |= X86_MMU_PG_G;




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
            DEBUG_ASSERT((*d != X86_MMU_CLEAR) && "Page unmapped");


            //* Set Page Type
            switch((flags & ARCH_VMM_MAP_TYPE_MASK)) {

                case ARCH_VMM_MAP_TYPE_PAGE:
                    b |= X86_MMU_PG_AP_TP_PAGE;
                    break;

                case ARCH_VMM_MAP_TYPE_UNIQUE:
                    b |= X86_MMU_PG_AP_TP_UNIQUE;
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


            *d = (*d & X86_MMU_ADDRESS_MASK) | b;


#if defined(DEBUG) && DEBUG_LEVEL >= 4
            //kprintf("arch_vmm_mprotect(): virtaddr(%p) physaddr(%p) flags(%p)\n", s, *d & X86_MMU_ADDRESS_MASK, b);
#endif

        }


        __asm__ __volatile__ ("invlpg (%0)" :: "r"(s) : "memory");
        
    }

    spinlock_unlock(&space->lock);

    return virtaddr;

}






/*!
 * @brief arch_vmm_access().
 *        Permission check for a virtual address.
 * 
 * @param space: address space.
 * @param virtaddr: virtual address.
 * @param mode: access mode.
 */
int arch_vmm_access(vmm_address_space_t* space, uintptr_t virtaddr, int mode) {

    DEBUG_ASSERT(space);
    //DEBUG_ASSERT(mode);


    uintptr_t s = virtaddr;
    int e = 0;


    if(s & (X86_MMU_PAGESIZE - 1))
        s = (s & ~(X86_MMU_PAGESIZE - 1));


    spinlock_lock(&space->lock);


    {

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

            }

        }

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

        }

#endif

        /* Page Table */
        {

            if(mode & R_OK)
                if(!(*d & X86_MMU_PG_P))
                    if((*d & X86_MMU_PG_AP_TP_MASK) != X86_MMU_PG_AP_TP_COW)
                        e = -1;

#if defined(__x86_64__)
            if(mode & X_OK)
                if( (*d & X86_MMU_PT_NX) && !(*d & (1ULL << 47)))
                    e = -1;
#endif

            if(mode & W_OK)
                if(!(*d & X86_MMU_PG_RW))
                    if((*d & X86_MMU_PG_AP_TP_MASK) != X86_MMU_PG_AP_TP_COW)
                        e = -1;
                    
        }

    }

    spinlock_unlock(&space->lock);

    return e;

}



void arch_vmm_clone(vmm_address_space_t* dest, vmm_address_space_t* src) {

    DEBUG_ASSERT(src);
    DEBUG_ASSERT(src->pm);
    DEBUG_ASSERT(dest);

    if(dest->pm == 0UL)
        dest->pm = alloc_page(X86_MMU_PAGESIZE, 1);

    


    void __clone_pt(uintptr_t __s, uintptr_t __d, int level) {

        DEBUG_ASSERT(__s);
        DEBUG_ASSERT(__d);


        x86_page_t* s = (x86_page_t*) arch_vmm_p2v(__s, ARCH_VMM_AREA_HEAP);
        x86_page_t* d = (x86_page_t*) arch_vmm_p2v(__d, ARCH_VMM_AREA_HEAP);

        int i;
        for(i = 0; i < X86_MMU_PT_ENTRIES; i++) {

            //? Skip unallocated pages
            if(s[i] == X86_MMU_CLEAR)
                continue;

            //? Skip unique pages
            if((s[i] & X86_MMU_PG_AP_TP_MASK) == X86_MMU_PG_AP_TP_UNIQUE)
                continue;


            if((s[i] & X86_MMU_PG_PS) || (level == 1))
                d[i] = s[i];

            else {

                d[i] = alloc_page(X86_MMU_PAGESIZE, 1) | (s[i] & ~X86_MMU_ADDRESS_MASK) | X86_MMU_PG_AP_PFB;

                __clone_pt(((uintptr_t) s[i]) & X86_MMU_ADDRESS_MASK, ((uintptr_t) d[i]) & X86_MMU_ADDRESS_MASK, level - 1);

            }

        }

    }

#if defined(__x86_64__)

    __lock(&src->lock,
        __clone_pt(src->pm, dest->pm, 4));

#elif defined(__i386__)

    __lock(&src->lock,
        __clone_pt(src->pm, dest->pm, 2));

#endif


    src->refcount++;
    dest->size = 0;
    dest->refcount = 1;
    
    spinlock_init(&dest->lock);

}




void pagefault_handle(interrupt_frame_t* frame) {


#if defined(DEBUG) && DEBUG_LEVEL >= 2

    #define PFE(reason)     \
        { kprintf("x86-pfe: FAULT! address(%p): %s\n", x86_get_cr2(), reason); goto pfe; }

#else

    #define PFE(reason)     \
        { goto pfe; }

#endif



    uintptr_t pm = x86_get_cr3();
    
    if(unlikely(!pm))
        PFE("no memory mapping");


    uintptr_t pagesize = X86_MMU_PAGESIZE;
    uintptr_t s = x86_get_cr2();



    {

        x86_page_t* d;


#if defined(__x86_64__)

        /* CR3-L4 */ 
        {
            d = &((x86_page_t*) arch_vmm_p2v(pm, ARCH_VMM_AREA_HEAP)) [(s >> 39) & 0x1FF];
        }

        /* PML4-L3 */
        {
            if(*d == X86_MMU_CLEAR)
                PFE("PML4-L3 doesn't not exist");

            d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 30) & 0x1FF];
        }


        /* HUGE_1GB */
        if(!(*d & X86_MMU_PG_PS)) {

            /* PDP-L2 */
            {
                if(*d == X86_MMU_CLEAR)
                    PFE("PDP-L2 doesn't not exist");

                d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 21) & 0x1FF];
            }

            /* HUGE_2MB */
            if(!(*d & X86_MMU_PG_PS)) {

                /* PD-L1 */
                {
                    if(*d == X86_MMU_CLEAR)
                        PFE("PD-L1 doesn't not exist");

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
                if(*d == X86_MMU_CLEAR)
                    PFE("PD-L1 doesn't not exist");

                d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 12) & 0x3FF];
            }

        } else
            pagesize = X86_MMU_HUGE_2MB_PAGESIZE;

#endif

        /* Page Table */
        {

            if(*d == X86_MMU_CLEAR)
                PFE("page not present");

            // TODO: implement X86_MMU_PG_AP_TP_MMAP
            if(!(*d & X86_MMU_PG_AP_TP_COW))
                PFE("page fault cannot be handled");


            if((*d & X86_MMU_ADDRESS_MASK) != 0)
                { PFE("Copy-on-write not yet supported"); } // TODO: implement copy on write
            else
                *d = alloc_page(pagesize, 0) | X86_MMU_PG_P 
                                             | X86_MMU_PG_AP_PFB 
                                             | X86_MMU_PG_AP_TP_PAGE
                                             | ((*d & ~X86_MMU_ADDRESS_MASK) & ~(X86_MMU_PG_AP_TP_MASK));

        }

    }


#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("x86-pfe: handled page fault! cs(%p), ip(%p), sp(%p), cr2(%p)\n", frame->cs, frame->ip, frame->user_sp, x86_get_cr2());
#endif

    return;


pfe:
    kpanicf("x86-pfe: PANIC! errno(%p), cs(%p), ip(%p), sp(%p), cr2(%p)\n", frame->errno, frame->cs, frame->ip, frame->user_sp, x86_get_cr2());

}