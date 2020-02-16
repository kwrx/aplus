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
                                                                        
#include <aplus/core/base.h>
#include <aplus/core/debug.h>
#include <aplus/core/ipc.h>
#include <aplus/core/multiboot.h>
#include <aplus/core/mm/pmm.h>


#define PML2_PAGESIZE               (128 * 1024 * 1024)     //? 128MiB
#define PML1_PAGESIZE               (4096)                  //? 4KiB

#define PML2_MAX_ENTRIES            (4096)
#define PML1_MAX_ENTRIES            (4096)


/*!
 * @brief pml2_bitmap[].
 *        Physical Page Map Level 2.
 * 
 * // TODO: Insert description
 */
static uintptr_t pml2_bitmap[PML2_MAX_ENTRIES];

/*!
 * @brief pml2_pusage[].
 *        Number of allocated pages in Page Map Level 1.
 */
static uint16_t pml2_pusage[PML2_MAX_ENTRIES];

/*!
 * @brief pml2_lock[].
 *        Array of spinlock for each page map.
 */
static spinlock_t pml2_lock[PML2_MAX_ENTRIES];

/*!
 * @brief pml1_first_bitmap[].
 *        First Page Map Bitmap (0-128Mib)
 */
static uint64_t pml1_first_bitmap[PML1_MAX_ENTRIES / sizeof(uint64_t)];

/*!
 * @brief pmm_max_memory.
 *        Max physical memory available.
 */
static uintptr_t pmm_max_memory = 0;




/*!
 * @brief pmm_claim_area().
 *        Mark user defined area as reserved.
 * 
 * @param physaddr: Physical Address of Memory Area.
 * @param size:     Size of Memory Area in bytes.
 */
void pmm_claim_area(uintptr_t physaddr, size_t size) {

    if(physaddr & PML1_PAGESIZE)
        physaddr &= ~(PML1_PAGESIZE - 1);

    if(size & PML1_PAGESIZE)
        size = (size & ~(PML1_PAGESIZE - 1)) + PML1_PAGESIZE;


    if(physaddr + size > pmm_max_memory)
        kpanicf("pmm: PANIC! Memory Area (%p-%p) is greater than max memory available (%p)\n", physaddr, physaddr + size, pmm_max_memory);



    for(uint64_t p = physaddr; p < (physaddr + size); p += PML1_PAGESIZE) {

        int pml2_index = p >> 27;

        BUG_ON(pml2_index < PML2_MAX_ENTRIES);
        BUG_ON(pml2_bitmap[pml2_index] != 0);


        uint64_t* pml1_bitmap = (uint64_t*) pml2_bitmap[pml2_index];

        __lock(&pml2_lock[pml2_index], {

            pml1_bitmap[p / 64] |= (1 << (p % 64));
            pml2_pusage[pml2_index]++;

        });

    }

}



/*!
 * @brief pmm_unclaim_area().
 *        Mark user defined area as free.
 * 
 * @param physaddr: Physical Address of Memory Area.
 * @param size:     Size of Memory Area in bytes.
 */
void pmm_unclaim_area(uintptr_t physaddr, size_t size) {

    if(physaddr & PML1_PAGESIZE)
        physaddr &= ~(PML1_PAGESIZE - 1);

    if(size & PML1_PAGESIZE)
        size = (size & ~(PML1_PAGESIZE - 1)) + PML1_PAGESIZE;


    if(physaddr + size > pmm_max_memory)
        kpanicf("pmm: PANIC! Memory Area (%p-%p) is greater than max memory available (%p)\n", physaddr, physaddr + size, pmm_max_memory);



    for(uint64_t p = physaddr; p < (physaddr + size); p += PML1_PAGESIZE) {

        int pml2_index = p >> 27;

        BUG_ON(pml2_index < PML2_MAX_ENTRIES);
        BUG_ON(pml2_bitmap[pml2_index] != 0);


        uint64_t* pml1_bitmap = (uint64_t*) pml2_bitmap[pml2_index];

        __lock(&pml2_lock[pml2_index], {

            pml1_bitmap[p / 64] &= ~(1 << (p % 64));
            pml2_pusage[pml2_index]--;

        });

    }

}



void pmm_init(uintptr_t max_memory) {

    DEBUG_ASSERT(max_memory);
    DEBUG_ASSERT(max_memory >= (16 * 1024 * 1024));

    pmm_max_memory = max_memory;


    int i;
    for(i = 0; i < PML2_MAX_ENTRIES; i++) {
        pml2_bitmap[i] = 0;
        pml2_pusage[i] = 0;
    }

    pml2_bitmap[0] = (uintptr_t) &pml1_first_bitmap;


    //! Claim Boot Memory Map areas
    for(i = 0; i < core->mmap.count; i++) {

        if(core->mmap.ptr[i].type == MULTIBOOT_MEMORY_AVAILABLE)
            continue;

        if(core->mmap.ptr[i].address > pmm_max_memory)
            continue;

        if(core->mmap.ptr[i].address + core->mmap.ptr[i].length > pmm_max_memory)
            continue;


#if defined(DEBUG) && DEBUG_LEVEL >= 1
        kprintf("pmm: claim physical memory area %p-%p\n", core->mmap.ptr[i].address,
                                                           core->mmap.ptr[i].address + core->mmap.ptr[i].length);
#endif

        pmm_claim_area(core->mmap.ptr[i].address, core->mmap.ptr[i].length);

    }


    //! Claim lower memory
    extern int end;
    pmm_claim_area(0, arch_kernel_v2p((uintptr_t) &end));


    //! Claim other page map memory blocks
    for(i = 0; i < PML2_MAX_ENTRIES; i++) {

        if((i * PML2_PAGESIZE) >= pmm_max_memory)
            break;

        //pml2_bitmap[i] = arch_heap_p2v(pmm_alloc_block());
        spinlock_init(&pml2_lock[i]);

    }


    kprintf("pmm: physical memory: %d KB\n", pmm_max_memory / 1024);

}