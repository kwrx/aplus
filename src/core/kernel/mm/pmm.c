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
                                                                        
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/multiboot.h>
#include <aplus/memory.h>

#include <hal/cpu.h>
#include <hal/interrupt.h>
#include <hal/vmm.h>



/*!
 * @brief pml2_bitmap[].
 *        Physical Page Map Level 2.
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
static uint64_t pml1_first_bitmap[PML1_MAX_ENTRIES];

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
void pmm_claim_area(uintptr_t physaddr, uintptr_t size) {

    DEBUG_ASSERT(size);


    uintptr_t end = physaddr + size;

    if(physaddr & (PML1_PAGESIZE - 1))
        physaddr &= ~(PML1_PAGESIZE - 1);

    if(end & (PML1_PAGESIZE - 1))
        end = (end & ~(PML1_PAGESIZE - 1)) + PML1_PAGESIZE;


    if(end > pmm_max_memory)
        kpanicf("pmm: PANIC! Memory Area (%p-%p) is greater than max memory available (%p)\n", physaddr, end, pmm_max_memory);



    for(uintptr_t p = physaddr; p < end; p += PML1_PAGESIZE) {

        uint64_t pml2_index = (p >> 27);
        uint64_t pml1_index = (p & 0x07FFFFFF) / PML1_PAGESIZE;

        BUG_ON(pml2_index < PML2_MAX_ENTRIES);
        BUG_ON(pml1_index < PML1_MAX_ENTRIES * 64);
        BUG_ON(pml2_bitmap[pml2_index] != 0);


        uint64_t* pml1_bitmap = (uint64_t*) pml2_bitmap[pml2_index];

        __lock(&pml2_lock[pml2_index], {

            pml1_bitmap[pml1_index / 64] |= (1ULL << (pml1_index % 64));
            pml2_pusage[pml2_index]++;

        });

    }

#if defined(DEBUG) && DEBUG_LEVEL >= 5
        kprintf("pmm: claim physical memory area %p-%p\n", physaddr, end);
#endif

}



/*!
 * @brief pmm_unclaim_area().
 *        Mark user defined area as free.
 * 
 * @param physaddr: Physical Address of Memory Area.
 * @param size:     Size of Memory Area in bytes.
 */
void pmm_unclaim_area(uintptr_t physaddr, size_t size) {

    DEBUG_ASSERT(size);


    uintptr_t end = physaddr + size;

    if(physaddr & (PML1_PAGESIZE - 1))
        physaddr &= ~(PML1_PAGESIZE - 1);

    if(end & (PML1_PAGESIZE - 1))
        end = (end & ~(PML1_PAGESIZE - 1)) + PML1_PAGESIZE;


    if(end > pmm_max_memory)
        kpanicf("pmm: PANIC! Memory Area (%p-%p) is greater than max memory available (%p)\n", physaddr, end, pmm_max_memory);



    for(uint64_t p = physaddr; p < end; p += PML1_PAGESIZE) {

        uint64_t pml2_index = (p >> 27);
        uint64_t pml1_index = (p & 0x07FFFFFF) / PML1_PAGESIZE;

        BUG_ON(pml2_index < PML2_MAX_ENTRIES);
        BUG_ON(pml1_index < PML1_MAX_ENTRIES * 64);
        BUG_ON(pml2_bitmap[pml2_index] != 0);


        uint64_t* pml1_bitmap = (uint64_t*) pml2_bitmap[pml2_index];

        __lock(&pml2_lock[pml2_index], {

            pml1_bitmap[pml1_index / 64] &= ~(1ULL << (pml1_index % 64));
            pml2_pusage[pml2_index]--;

        });

    }

#if defined(DEBUG) && DEBUG_LEVEL >= 5
        kprintf("pmm: unclaim physical memory area %p-%p\n", physaddr, end);
#endif

}



/*!
 * @brief pmm_alloc_block().
 *        Allocate a physical block of PML1_PAGESIZE bytes.
 */
uintptr_t pmm_alloc_block() {

    uint64_t r = -1;
    uint64_t i, j, q;

    for(i = 0; i < PML2_MAX_ENTRIES; i++) {

        if(pml2_bitmap[i] == 0)
            break;

        if(pml2_pusage[i] == PML2_MAX_ENTRIES << 3)
            continue;


        uint64_t* pml1_bitmap = (uint64_t*) pml2_bitmap[i];


        for(q = 0; q < PML1_MAX_ENTRIES; q++) {

            if(unlikely(pml1_bitmap[q] == 0xFFFFFFFFFFFFFFFFULL))
                continue;


            __lock(&pml2_lock[q], {
                
                for(j = 0; j < 64; j++) {

                    if(unlikely(pml1_bitmap[q] & (1ULL << j)))
                        continue;

                        
                    pml1_bitmap[q] |= (1ULL << j);
                    pml2_pusage[q]++;

                    r = (i * PML2_PAGESIZE) + (((q << 6ULL) + j) * PML1_PAGESIZE);
                    break;

                }

            });

            if(unlikely(r != -1))
                goto end;

        }
    
    }


end:

#if defined(DEBUG) && DEBUG_LEVEL >= 5
    kprintf("pmm: pmm_alloc_block() at %p\n", r);
#endif

    return r;

}



/*!
 * @brief pmm_alloc_blocks().
 *        Allocate physical blocks of (n * PML1_PAGESIZE) bytes.
 * 
 * @param blkno: Number of blocks to allocate.
 */
uintptr_t pmm_alloc_blocks(size_t blkno) {

    DEBUG_ASSERT(blkno);


    uint64_t r = -1;
    uint64_t c = 0;
    uint64_t i, j, q;

    for(i = 0; i < PML2_MAX_ENTRIES; i++) {

        if(pml2_bitmap[i] == 0)
            break;

        if(pml2_pusage[i] >= (PML2_MAX_ENTRIES << 3) - blkno)
            continue;


        uint64_t* pml1_bitmap = (uint64_t*) pml2_bitmap[i];


        for(q = 0; q < PML1_MAX_ENTRIES; q++) {

            if(unlikely(pml1_bitmap[q] == 0xFFFFFFFFFFFFFFFFULL))
                { c = 0; continue; }


            __lock(&pml2_lock[q], {
                
                for(j = 0; j < 64; j++) {

                    if(unlikely(pml1_bitmap[q] & (1ULL << j)))
                        { c = 0; continue; }

                    if(c == 0)
                        r = (i * PML2_PAGESIZE) + (((q << 6ULL) + j) * PML1_PAGESIZE);

                    if(++c == blkno)
                        break;

                }

            });

            if(unlikely(c == blkno))
                goto end;

        }
    
    }


end:

    pmm_claim_area(r, blkno * PML1_PAGESIZE);

#if defined(DEBUG) && DEBUG_LEVEL >= 5
    kprintf("pmm: pmm_alloc_blocks(%d) at %p-%p\n", blkno, r, r + (blkno * PML1_PAGESIZE));
#endif

    return r;

}




/*!
 * @brief pmm_alloc_blocks_aligned().
 *        Allocate physical blocks of (n * PML1_PAGESIZE) bytes at aligned address.
 * 
 * @param blkno: Number of blocks to allocate.
 * @param align: Address alignment.
 */
uintptr_t pmm_alloc_blocks_aligned(size_t blkno, uintptr_t align) {

    DEBUG_ASSERT(blkno);
    DEBUG_ASSERT(align);
    DEBUG_ASSERT((align & (PML1_PAGESIZE - 1)) == 0);


    uint64_t r = -1;
    uint64_t c = 0;
    uint64_t i, j, q;

    for(i = 0; i < PML2_MAX_ENTRIES; i++) {

        if(pml2_bitmap[i] == 0)
            break;

        if(pml2_pusage[i] >= (PML2_MAX_ENTRIES << 3) - blkno)
            continue;


        uint64_t* pml1_bitmap = (uint64_t*) pml2_bitmap[i];


        for(q = 0; q < PML1_MAX_ENTRIES; q++) {

            if(unlikely(pml1_bitmap[q] == 0xFFFFFFFFFFFFFFFFULL))
                { c = 0; continue; }


            __lock(&pml2_lock[q], {
                
                for(j = 0; j < 64; j++) {

                    if(unlikely(pml1_bitmap[q] & (1ULL << j)))
                        { c = 0; continue; }

                    if(c == 0)
                        if((r = (i * PML2_PAGESIZE) + (((q << 6ULL) + j) * PML1_PAGESIZE)) & (align - 1))
                            continue;

                    if(++c == blkno)
                        break;

                }

            });

            if(unlikely(c == blkno))
                goto end;

        }
    
    }


end:

    pmm_claim_area(r, blkno * PML1_PAGESIZE);

#if defined(DEBUG) && DEBUG_LEVEL >= 5
    kprintf("pmm: pmm_alloc_blocks_aligned(%d, %p) at %p-%p\n", blkno, align, r, r + (blkno * PML1_PAGESIZE));
#endif

    return r;

}





/*!
 * @brief pmm_free_block().
 *        Frees a physical block of PML1_PAGESIZE bytes.
 * 
 * @param address: Physical address of block.
 */
void pmm_free_block(uintptr_t address) {
    pmm_unclaim_area(address, PML1_PAGESIZE);
}



/*!
 * @brief pmm_free_blocks().
 *        Frees physical blocks of (n * PML1_PAGESIZE) bytes.
 * 
 * @param address: Physical address of block.
 * @param blkno: Number of blocks to free.
 */
void pmm_free_blocks(uintptr_t address, size_t blkno) {

    DEBUG_ASSERT(blkno);

    pmm_unclaim_area(address, PML1_PAGESIZE * blkno);

}



/*!
 * @brief pmm_get_used_memory().
 *        Get Physical Memory used by system.
 */
uint64_t pmm_get_used_memory() {

    uint64_t sum = 0;

    for(int i = 0; i < PML2_MAX_ENTRIES; i++)
        sum += pml2_pusage[i];

    return sum * PML1_PAGESIZE;

}



/*!
 * @brief pmm_init().
 *        Initialize Physical Memory Manager.
 * 
 * @param max_memory: Max amount of physical memory.
 */
void pmm_init(uintptr_t max_memory) {

    DEBUG_ASSERT(max_memory);
    DEBUG_ASSERT(max_memory >= (16 * 1024 * 1024));

    pmm_max_memory = max_memory;


    int i;
    for(i = 0; i < PML2_MAX_ENTRIES; i++) {
        pml2_bitmap[i] = 0;
        pml2_pusage[i] = 0;
    }


    for(i = 0; i < PML1_MAX_ENTRIES; i++)
        pml1_first_bitmap[i] = 0;

    pml2_bitmap[0] = (uintptr_t) &pml1_first_bitmap;



    //! Claim Boot Memory Map areas
    for(i = 0; i < core->mmap.count; i++) {

        if(core->mmap.ptr[i].type == MULTIBOOT_MEMORY_AVAILABLE)
            continue;

        if(core->mmap.ptr[i].address > pmm_max_memory)
            continue;

        if(core->mmap.ptr[i].address + core->mmap.ptr[i].length > pmm_max_memory)
            continue;


#if defined(DEBUG) && DEBUG_LEVEL >= 2
        kprintf("mmap: address(%p) size(%p) type(%p)\n", core->mmap.ptr[i].address,
                                                         core->mmap.ptr[i].length,
                                                         core->mmap.ptr[i].type);
#endif

        pmm_claim_area(core->mmap.ptr[i].address, core->mmap.ptr[i].length);

    }


    //! Claim lower memory
    extern int end;
    pmm_claim_area(0, arch_vmm_v2p((uintptr_t) &end, ARCH_VMM_AREA_KERNEL));


    //! Claim other page map memory blocks
    for(i = 1; i < PML2_MAX_ENTRIES; i++) {

        if(((uintptr_t) i * PML2_PAGESIZE) >= pmm_max_memory)
            break;


        uint64_t* b = (uint64_t*) arch_vmm_p2v(pmm_alloc_block(), ARCH_VMM_AREA_HEAP);

        int j;
        for(j = 0; j < PML1_MAX_ENTRIES; j++)
            b[j] = 0;

        spinlock_init(&pml2_lock[i]);
        pml2_bitmap[i] = (uintptr_t) b;

    }


#if defined(DEBUG)
    kprintf("pmm: physical memory: %d KB\n", pmm_max_memory / 1024);
#endif

}