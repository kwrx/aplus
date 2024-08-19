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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/multiboot.h>



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
 *        First preallocated Page Map Bitmap (0-2GiB)
 */
static uint64_t pml1_first_preallocated_bitmaps[PML1_MAX_ENTRIES * PML1_PREALLOCATED_BITMAPS];

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

    if (physaddr & (PML1_PAGESIZE - 1))
        physaddr &= ~(PML1_PAGESIZE - 1);

    if (end & (PML1_PAGESIZE - 1))
        end = (end & ~(PML1_PAGESIZE - 1)) + PML1_PAGESIZE;


    if (end > pmm_max_memory) {
        kpanicf("pmm: PANIC! Memory Area (0x%lX-0x%lX) is greater than max memory available (%ld)\n", physaddr, end, pmm_max_memory);
    }


    for (uintptr_t p = physaddr; p < end; p += PML1_PAGESIZE) {

        uint64_t pml2_index = (p >> 27);
        uint64_t pml1_index = (p & 0x07FFFFFF) / PML1_PAGESIZE;

        DEBUG_ASSERT(pml2_index < PML2_MAX_ENTRIES);
        DEBUG_ASSERT(pml1_index < PML1_MAX_ENTRIES * 64);
        DEBUG_ASSERT(pml2_bitmap[pml2_index] != 0);


        uint64_t* pml1_bitmap = (uint64_t*)pml2_bitmap[pml2_index];

        scoped_lock(&pml2_lock[pml2_index]) {
            pml1_bitmap[pml1_index / 64] |= (1ULL << (pml1_index % 64));
            pml2_pusage[pml2_index]++;
        }
    }

#if DEBUG_LEVEL_TRACE
    // // kprintf("pmm: claim physical memory area %p-%p\n", (void*) physaddr, (void*) end);
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

    if (physaddr & (PML1_PAGESIZE - 1))
        physaddr &= ~(PML1_PAGESIZE - 1);

    if (end & (PML1_PAGESIZE - 1))
        end = (end & ~(PML1_PAGESIZE - 1)) + PML1_PAGESIZE;


    if (end > pmm_max_memory) {
        kpanicf("pmm: PANIC! Memory Area (0x%lX-0x%lX) is greater than max memory available (%ld)\n", physaddr, end, pmm_max_memory);
    }


    for (uint64_t p = physaddr; p < end; p += PML1_PAGESIZE) {

        uint64_t pml2_index = (p >> 27);
        uint64_t pml1_index = (p & 0x07FFFFFF) / PML1_PAGESIZE;

        DEBUG_ASSERT(pml2_index < PML2_MAX_ENTRIES);
        DEBUG_ASSERT(pml1_index < PML1_MAX_ENTRIES * 64);
        DEBUG_ASSERT(pml2_bitmap[pml2_index] != 0);


        uint64_t* pml1_bitmap = (uint64_t*)pml2_bitmap[pml2_index];

        scoped_lock(&pml2_lock[pml2_index]) {
            pml1_bitmap[pml1_index / 64] &= ~(1ULL << (pml1_index % 64));
            pml2_pusage[pml2_index]--;
        }
    }

#if DEBUG_LEVEL_TRACE
    // // kprintf("pmm: unclaim physical memory area %p-%p\n", (void*) physaddr, (void*) end);
#endif
}



/*!
 * @brief pmm_alloc_block().
 *        Allocate a physical block of PML1_PAGESIZE bytes.
 */
uintptr_t pmm_alloc_block() {

    uint64_t r = -1ULL;
    uint64_t i, j, q;

    for (i = 0; i < PML2_MAX_ENTRIES; i++) {

        if (pml2_bitmap[i] == 0)
            break;

        if (pml2_pusage[i] >= (PML1_MAX_ENTRIES << 6) - 1)
            continue;


        uint64_t* pml1_bitmap = (uint64_t*)pml2_bitmap[i];


        for (q = 0; q < PML1_MAX_ENTRIES; q++) {

            if (unlikely(pml1_bitmap[q] == 0xFFFFFFFFFFFFFFFFULL))
                continue;


            scoped_lock(&pml2_lock[i]) {
                for (j = 0; j < 64; j++) {

                    if (unlikely(pml1_bitmap[q] & (1ULL << j)))
                        continue;


                    pml1_bitmap[q] |= (1ULL << j);
                    pml2_pusage[i]++;

                    r = (i * PML2_PAGESIZE) + (((q << 6ULL) + j) * PML1_PAGESIZE);
                    break;
                }
            }

            if (unlikely(r != -1))
                break;
        }

        if (unlikely(r != -1))
            break;
    }



    DEBUG_ASSERT(r != -1ULL);

#if DEBUG_LEVEL_TRACE
    // // kprintf("pmm: pmm_alloc_block() at %p\n", r);
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


    uint64_t r = -1ULL;
    uint64_t c = 0;
    uint64_t i, j, q;

    for (i = 0; i < PML2_MAX_ENTRIES; i++) {

        if (pml2_bitmap[i] == 0) {
            r = -1ULL;
            break;
        }

        if (pml2_pusage[i] >= (PML1_MAX_ENTRIES << 6) - blkno) {
            c = 0;
            continue;
        }


        uint64_t* pml1_bitmap = (uint64_t*)pml2_bitmap[i];


        for (q = 0; q < PML1_MAX_ENTRIES; q++) {

            if (unlikely(pml1_bitmap[q] == 0xFFFFFFFFFFFFFFFFULL)) {
                c = 0;
                continue;
            }


            scoped_lock(&pml2_lock[i]) {
                for (j = 0; j < 64; j++) {

                    if (unlikely(pml1_bitmap[q] & (1ULL << j))) {
                        c = 0;
                        continue;
                    }

                    if (c == 0)
                        r = (i * PML2_PAGESIZE) + (((q << 6ULL) + j) * PML1_PAGESIZE);

                    if (++c == blkno)
                        break;
                }
            }

            if (unlikely(c == blkno))
                break;
        }

        if (unlikely(c == blkno))
            break;
    }



    DEBUG_ASSERT(r != -1ULL);

    pmm_claim_area(r, blkno * PML1_PAGESIZE);

#if DEBUG_LEVEL_TRACE
    // // kprintf("pmm: pmm_alloc_blocks(%d) at %p-%p\n", blkno, r, r + (blkno * PML1_PAGESIZE));
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


    uint64_t r = -1ULL;
    uint64_t c = 0;
    uint64_t i, j, q;

    for (i = 0; i < PML2_MAX_ENTRIES; i++) {

        if (pml2_bitmap[i] == 0) {
            r = -1ULL;
            break;
        }

        if (pml2_pusage[i] >= (PML1_MAX_ENTRIES << 6) - blkno) {
            c = 0;
            continue;
        }



        uint64_t* pml1_bitmap = (uint64_t*)pml2_bitmap[i];


        for (q = 0; q < PML1_MAX_ENTRIES; q++) {

            if (unlikely(pml1_bitmap[q] == 0xFFFFFFFFFFFFFFFFULL)) {
                c = 0;
                continue;
            }


            scoped_lock(&pml2_lock[i]) {
                for (j = 0; j < 64; j++) {

                    if (unlikely(pml1_bitmap[q] & (1ULL << j))) {
                        c = 0;
                        continue;
                    }

                    if (c == 0)
                        if ((r = (i * PML2_PAGESIZE) + (((q << 6ULL) + j) * PML1_PAGESIZE)) & (align - 1))
                            continue;

                    if (++c == blkno)
                        break;
                }
            }

            if (unlikely(c == blkno))
                break;
        }

        if (unlikely(c == blkno))
            break;
    }



    DEBUG_ASSERT(r != -1ULL);

    pmm_claim_area(r, blkno * PML1_PAGESIZE);

#if DEBUG_LEVEL_TRACE
    // // kprintf("pmm: pmm_alloc_blocks_aligned(%d, %p) at %p-%p\n", blkno, align, r, r + (blkno * PML1_PAGESIZE));
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

    for (int i = 0; i < PML2_MAX_ENTRIES; i++)
        sum += pml2_pusage[i];

    return sum * PML1_PAGESIZE;
}

/*!
 * @brief pmm_get_total_memory().
 *        Get Physical Memory size.
 */
uint64_t pmm_get_total_memory() {
    return pmm_max_memory;
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



#if DEBUG_LEVEL_TRACE
    for (size_t i = 0; i < core->mmap.count; i++) {

        kprintf("pmm: area #%zd address(0x%16lX-0x%16lX) type(%ld)\n", i, core->mmap.ptr[i].address, core->mmap.ptr[i].address + core->mmap.ptr[i].length, core->mmap.ptr[i].type);
    }
#endif



    for (size_t i = 0; i < PML2_MAX_ENTRIES; i++) {

        pml2_bitmap[i] = 0;
        pml2_pusage[i] = 0;

        if (i <= (pmm_max_memory / PML2_PAGESIZE)) {
            spinlock_init_with_flags(&pml2_lock[i], SPINLOCK_FLAGS_CPU_OWNER);
        }
    }


    for (size_t i = 0; i < PML1_MAX_ENTRIES * PML1_PREALLOCATED_BITMAPS; i++) {
        pml1_first_preallocated_bitmaps[i] = 0;
    }

    for (size_t i = 0; i < PML1_PREALLOCATED_BITMAPS; i++) {

        if (i * PML2_PAGESIZE >= pmm_max_memory)
            break;

        pml2_bitmap[i] = (uintptr_t)&pml1_first_preallocated_bitmaps[PML1_MAX_ENTRIES * i];
    }


    extern int end;


    // Claim lower memory

    pmm_claim_area(0, arch_vmm_v2p((uintptr_t)&end, ARCH_VMM_AREA_KERNEL));


    // Claim Boot Memory Map areas in the first bitmap.

    for (size_t i = 0; i < core->mmap.count; i++) {

        if (core->mmap.ptr[i].type == MULTIBOOT_MEMORY_AVAILABLE)
            continue;

        if (core->mmap.ptr[i].address > MIN(PML2_PAGESIZE * PML1_PREALLOCATED_BITMAPS, pmm_max_memory))
            continue;

        if (core->mmap.ptr[i].address + core->mmap.ptr[i].length > MIN(PML2_PAGESIZE * PML1_PREALLOCATED_BITMAPS, pmm_max_memory))
            continue;


        pmm_claim_area(core->mmap.ptr[i].address, core->mmap.ptr[i].length);
    }


    //  Alloc other page map memory bitmaps.

    for (size_t i = PML1_PREALLOCATED_BITMAPS; i < PML2_MAX_ENTRIES && (i * PML2_PAGESIZE) < pmm_max_memory; i++) {


        uintptr_t phys = pmm_alloc_block();

        if (unlikely(phys == -1ULL)) {
            kpanicf("pmm: failed to allocate bitmap for PML2 entry %ld\n", i);
        }


        uintptr_t virt = arch_vmm_p2v(phys, ARCH_VMM_AREA_HEAP);

        if (unlikely(virt == -1ULL)) {
            kpanicf("pmm: failed to map bitmap for PML2 entry %ld\n", i);
        }

        memset((void*)virt, 0, PML1_MAX_ENTRIES * sizeof(uint64_t));


        pml2_bitmap[i] = virt;
    }


    // Claim other boot memory map areas.

    for (size_t i = 0; i < core->mmap.count; i++) {

        if (core->mmap.ptr[i].type == MULTIBOOT_MEMORY_AVAILABLE)
            continue;

        if (core->mmap.ptr[i].address < MIN(PML2_PAGESIZE * PML1_PREALLOCATED_BITMAPS, pmm_max_memory))
            continue;

        if (core->mmap.ptr[i].address + core->mmap.ptr[i].length < MIN(PML2_PAGESIZE * PML1_PREALLOCATED_BITMAPS, pmm_max_memory))
            continue;

        if (core->mmap.ptr[i].address + core->mmap.ptr[i].length > MAX(PML2_PAGESIZE * PML1_PREALLOCATED_BITMAPS, pmm_max_memory))
            continue;

        if (core->mmap.ptr[i].address > MAX(PML2_PAGESIZE * PML1_PREALLOCATED_BITMAPS, pmm_max_memory))
            continue;

        if (core->mmap.ptr[i].address + core->mmap.ptr[i].length > MAX(PML2_PAGESIZE * PML1_PREALLOCATED_BITMAPS, pmm_max_memory))
            continue;


        pmm_claim_area(core->mmap.ptr[i].address, core->mmap.ptr[i].length);
    }


#if DEBUG_LEVEL_INFO
    kprintf("pmm: physical memory: %ld KB\n", pmm_max_memory / 1024);
#endif
}


TEST(pmm_small_alloc_test, {
    uintptr_t b1 = pmm_alloc_block();
    uintptr_t b2 = pmm_alloc_block();
    uintptr_t b3 = pmm_alloc_block();
    uintptr_t b4 = pmm_alloc_block();


    DEBUG_ASSERT(b1 != -1ULL);
    DEBUG_ASSERT(b2 != -1ULL);
    DEBUG_ASSERT(b3 != -1ULL);
    DEBUG_ASSERT(b4 != -1ULL);


    pmm_free_block(b1);
    pmm_free_block(b3);


    uintptr_t b5 = pmm_alloc_block();
    uintptr_t b6 = pmm_alloc_block();

    DEBUG_ASSERT(b5 == b1);
    DEBUG_ASSERT(b6 == b3);


    pmm_free_block(b2);
    pmm_free_block(b4);
    pmm_free_block(b5);
    pmm_free_block(b6);
});
