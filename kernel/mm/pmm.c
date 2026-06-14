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



#define PMM_INVALID_ADDRESS       ((uintptr_t)-1ULL)
#define PMM_PAGES_PER_PML2_ENTRY  (PML1_MAX_ENTRIES << 6)
#define PMM_PREALLOCATED_END      (PML2_PAGESIZE * PML1_PREALLOCATED_BITMAPS)


/*!
 * @brief pml2_bitmap[].
 *        Physical Page Map Level 2.
 */
static uintptr_t pml2_bitmap[PML2_MAX_ENTRIES] = { 0 };

/*!
 * @brief pml2_pusage[].
 *        Number of allocated pages in Page Map Level 1.
 */
static uint16_t pml2_pusage[PML2_MAX_ENTRIES] = { 0 };

/*!
 * @brief pmm_lock.
 *        Serializes all bitmap operations and allocation scans.
 */
static spinlock_t pmm_lock = SPINLOCK_INIT_WITH_FLAGS(SPINLOCK_FLAGS_CPU_OWNER);

/*!
 * @brief pml1_first_bitmap[].
 *        First preallocated Page Map Bitmap (0-2GiB)
 */
static uint64_t pml1_first_preallocated_bitmaps[PML1_MAX_ENTRIES * PML1_PREALLOCATED_BITMAPS] = { 0 };

/*!
 * @brief pmm_max_memory.
 *        Max physical memory available.
 */
static uintptr_t pmm_max_memory = 0;



static inline uintptr_t pmm_managed_end(void) {
    return pmm_max_memory & ~(PML1_PAGESIZE - 1);
}


static inline size_t pmm_region_capacity(size_t pml2_index) {

    uint64_t start = (uint64_t)pml2_index * PML2_PAGESIZE;
    uint64_t end   = pmm_managed_end();

    if (start >= end)
        return 0;

    return MIN(end - start, PML2_PAGESIZE) / PML1_PAGESIZE;
}


static inline bool pmm_range_end(uintptr_t address, size_t size, uintptr_t* end) {

    if (!size || address > UINTPTR_MAX - size)
        return false;

    *end = address + size;
    return true;
}


static inline bool pmm_page_used_locked(uintptr_t address) {

    size_t pml2_index = address / PML2_PAGESIZE;
    size_t pml1_index = (address % PML2_PAGESIZE) / PML1_PAGESIZE;

    if (pml2_index >= PML2_MAX_ENTRIES || pml2_bitmap[pml2_index] == 0 || pml1_index >= pmm_region_capacity(pml2_index))
        return true;

    uint64_t* pml1_bitmap = (uint64_t*)pml2_bitmap[pml2_index];
    return (pml1_bitmap[pml1_index / 64] & (1ULL << (pml1_index % 64))) != 0;
}


static inline void pmm_set_page_locked(uintptr_t address, bool used) {

    size_t pml2_index = address / PML2_PAGESIZE;
    size_t pml1_index = (address % PML2_PAGESIZE) / PML1_PAGESIZE;

    DEBUG_ASSERT(pml2_index < PML2_MAX_ENTRIES);
    DEBUG_ASSERT(pml2_bitmap[pml2_index] != 0);
    DEBUG_ASSERT(pml1_index < pmm_region_capacity(pml2_index));

    uint64_t* pml1_bitmap = (uint64_t*)pml2_bitmap[pml2_index];
    uint64_t mask         = 1ULL << (pml1_index % 64);
    bool current          = (pml1_bitmap[pml1_index / 64] & mask) != 0;

    if (current == used)
        return;

    if (used) {
        pml1_bitmap[pml1_index / 64] |= mask;
        pml2_pusage[pml2_index]++;
    } else {
        pml1_bitmap[pml1_index / 64] &= ~mask;
        pml2_pusage[pml2_index]--;
    }
}


static void pmm_claim_range_locked(uintptr_t start, uintptr_t end) {

    uintptr_t managed_end = pmm_managed_end();

    start &= ~(PML1_PAGESIZE - 1);

    if (end & (PML1_PAGESIZE - 1)) {
        if (end > UINTPTR_MAX - PML1_PAGESIZE)
            end = UINTPTR_MAX;
        else
            end = (end & ~(PML1_PAGESIZE - 1)) + PML1_PAGESIZE;
    }

    end = MIN(end, managed_end);

    for (uintptr_t p = start; p < end; p += PML1_PAGESIZE) {

        size_t pml2_index = p / PML2_PAGESIZE;

        if (pml2_index >= PML2_MAX_ENTRIES)
            break;

        if (pml2_bitmap[pml2_index] != 0)
            pmm_set_page_locked(p, true);
    }
}


static bool pmm_release_range_locked(uintptr_t start, uintptr_t end, bool strict) {

    uintptr_t managed_end = pmm_managed_end();

    if (start & (PML1_PAGESIZE - 1)) {
        if (start > UINTPTR_MAX - PML1_PAGESIZE) {
            DEBUG_ASSERT(0 && "Invalid PMM free address");
            return false;
        }

        start = (start & ~(PML1_PAGESIZE - 1)) + PML1_PAGESIZE;
    }

    end &= ~(PML1_PAGESIZE - 1);

    if (start >= end || start >= managed_end) {
        if (strict)
            DEBUG_ASSERT(0 && "Invalid PMM free range");

        return !strict;
    }

    if (strict && end > managed_end) {
        DEBUG_ASSERT(0 && "PMM free range exceeds managed memory");
        return false;
    }

    end = MIN(end, managed_end);

    if (strict) {
        for (uintptr_t p = start; p < end; p += PML1_PAGESIZE) {
            if (!pmm_page_used_locked(p)) {
                DEBUG_ASSERT(0 && "PMM double free");
                return false;
            }
        }
    }

    for (uintptr_t p = start; p < end; p += PML1_PAGESIZE) {

        size_t pml2_index = p / PML2_PAGESIZE;

        if (pml2_index >= PML2_MAX_ENTRIES)
            break;

        if (pml2_bitmap[pml2_index] != 0)
            pmm_set_page_locked(p, false);
    }

    return true;
}


static uintptr_t pmm_alloc_blocks_locked(size_t blkno, uintptr_t align) {

    if (!blkno || blkno > SIZE_MAX / PML1_PAGESIZE || !align || (align & (PML1_PAGESIZE - 1)) || (align & (align - 1)))
        return PMM_INVALID_ADDRESS;

    if (blkno > pmm_managed_end() / PML1_PAGESIZE)
        return PMM_INVALID_ADDRESS;

    uintptr_t candidate = PMM_INVALID_ADDRESS;
    size_t contiguous    = 0;

    for (size_t i = 0; i < PML2_MAX_ENTRIES; i++) {

        size_t capacity = pmm_region_capacity(i);

        if (!capacity)
            break;

        if (pml2_bitmap[i] == 0) {
            candidate  = PMM_INVALID_ADDRESS;
            contiguous = 0;
            continue;
        }

        if (pml2_pusage[i] == capacity) {
            candidate  = PMM_INVALID_ADDRESS;
            contiguous = 0;
            continue;
        }

        for (size_t j = 0; j < capacity; j++) {

            uintptr_t address = (i * PML2_PAGESIZE) + (j * PML1_PAGESIZE);

            if (pmm_page_used_locked(address)) {
                candidate  = PMM_INVALID_ADDRESS;
                contiguous = 0;
                continue;
            }

            if (!contiguous) {
                if (address & (align - 1))
                    continue;

                candidate = address;
            }

            if (++contiguous == blkno) {

                for (size_t page = 0; page < blkno; page++)
                    pmm_set_page_locked(candidate + (page * PML1_PAGESIZE), true);

                return candidate;
            }
        }
    }

    return PMM_INVALID_ADDRESS;
}


static void pmm_install_bitmap(size_t pml2_index, uintptr_t bitmap) {

    DEBUG_ASSERT(pml2_index < PML2_MAX_ENTRIES);
    DEBUG_ASSERT(bitmap);

    memset((void*)bitmap, 0xFF, PML1_MAX_ENTRIES * sizeof(uint64_t));

    pml2_bitmap[pml2_index] = bitmap;
    pml2_pusage[pml2_index] = pmm_region_capacity(pml2_index);
}


static void pmm_apply_memory_map(bool available, uintptr_t limit) {

    for (size_t i = 0; i < core->mmap.count; i++) {

        bool entry_available = core->mmap.ptr[i].type == MULTIBOOT_MEMORY_AVAILABLE;

        if (entry_available != available)
            continue;

        if (!core->mmap.ptr[i].length)
            continue;

        uintptr_t end;

        if (!pmm_range_end(core->mmap.ptr[i].address, core->mmap.ptr[i].length, &end))
            end = UINTPTR_MAX;

        end = MIN(end, limit);

        if (core->mmap.ptr[i].address >= end)
            continue;

        scoped_lock(&pmm_lock) {
            if (available)
                pmm_release_range_locked(core->mmap.ptr[i].address, end, false);
            else
                pmm_claim_range_locked(core->mmap.ptr[i].address, end);
        }
    }
}


#if defined(DEBUG)
static void pmm_validate_locked(void) {

    for (size_t i = 0; i < PML2_MAX_ENTRIES; i++) {

        size_t capacity = pmm_region_capacity(i);

        if (!capacity)
            break;

        DEBUG_ASSERT(pml2_bitmap[i]);

        size_t used              = 0;
        uint64_t* pml1_bitmap    = (uint64_t*)pml2_bitmap[i];

        for (size_t j = 0; j < capacity; j++)
            if (pml1_bitmap[j / 64] & (1ULL << (j % 64)))
                used++;

        DEBUG_ASSERT(used == pml2_pusage[i]);

        for (size_t j = capacity; j < PMM_PAGES_PER_PML2_ENTRY; j++)
            DEBUG_ASSERT(pml1_bitmap[j / 64] & (1ULL << (j % 64)));
    }
}
#endif



/*!
 * @brief pmm_claim_area().
 *        Mark user defined area as reserved.
 *
 * @param physaddr: Physical Address of Memory Area.
 * @param size:     Size of Memory Area in bytes.
 */
void pmm_claim_area(uintptr_t physaddr, uintptr_t size) {

    uintptr_t end = 0;

    if (!pmm_range_end(physaddr, size, &end)) {
        DEBUG_ASSERT(0 && "Invalid PMM claim range");
        return;
    }

    scoped_lock(&pmm_lock) {
        pmm_claim_range_locked(physaddr, end);
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

    uintptr_t end = 0;

    if (!pmm_range_end(physaddr, size, &end)) {
        DEBUG_ASSERT(0 && "Invalid PMM free range");
        return;
    }

    scoped_lock(&pmm_lock) {
        pmm_release_range_locked(physaddr, end, true);
    }
}



/*!
 * @brief pmm_alloc_block().
 *        Allocate a physical block of PML1_PAGESIZE bytes.
 */
uintptr_t pmm_alloc_block() {
    return pmm_alloc_blocks(1);
}



/*!
 * @brief pmm_alloc_blocks().
 *        Allocate physical blocks of (n * PML1_PAGESIZE) bytes.
 *
 * @param blkno: Number of blocks to allocate.
 */
uintptr_t pmm_alloc_blocks(size_t blkno) {

    uintptr_t address;

    scoped_lock(&pmm_lock) {
        address = pmm_alloc_blocks_locked(blkno, PML1_PAGESIZE);
    }

    return address;
}



/*!
 * @brief pmm_alloc_blocks_aligned().
 *        Allocate physical blocks of (n * PML1_PAGESIZE) bytes at aligned address.
 *
 * @param blkno: Number of blocks to allocate.
 * @param align: Address alignment.
 */
uintptr_t pmm_alloc_blocks_aligned(size_t blkno, uintptr_t align) {

    uintptr_t address;

    scoped_lock(&pmm_lock) {
        address = pmm_alloc_blocks_locked(blkno, align);
    }

    return address;
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

    if (!blkno || blkno > SIZE_MAX / PML1_PAGESIZE) {
        DEBUG_ASSERT(0 && "Invalid PMM free size");
        return;
    }

    pmm_unclaim_area(address, PML1_PAGESIZE * blkno);
}



/*!
 * @brief pmm_get_used_memory().
 *        Get Physical Memory used by system.
 */
uint64_t pmm_get_used_memory() {

    uint64_t sum = 0;

    scoped_lock(&pmm_lock) {
        for (size_t i = 0; i < PML2_MAX_ENTRIES; i++)
            sum += pml2_pusage[i];
    }

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

    PANIC_ASSERT(max_memory >= (16 * 1024 * 1024));
    PANIC_ASSERT((uint64_t)max_memory <= ((uint64_t)PML2_MAX_ENTRIES * PML2_PAGESIZE));

    pmm_max_memory = max_memory;
    spinlock_init_with_flags(&pmm_lock, SPINLOCK_FLAGS_CPU_OWNER);


#if DEBUG_LEVEL_TRACE
    for (size_t i = 0; i < core->mmap.count; i++) {
        kprintf("pmm: area #%zd address(0x%16lX-0x%16lX) type(%ld)\n", i, core->mmap.ptr[i].address, core->mmap.ptr[i].address + core->mmap.ptr[i].length, core->mmap.ptr[i].type);
    }
#endif


    for (size_t i = 0; i < PML2_MAX_ENTRIES; i++) {
        pml2_bitmap[i] = 0;
        pml2_pusage[i] = 0;
    }

    for (size_t i = 0; i < PML1_PREALLOCATED_BITMAPS && pmm_region_capacity(i); i++)
        pmm_install_bitmap(i, (uintptr_t)&pml1_first_preallocated_bitmaps[PML1_MAX_ENTRIES * i]);

    // Expose usable low memory so it can fund the remaining bitmap pages.

    pmm_apply_memory_map(true, MIN(PMM_PREALLOCATED_END, pmm_managed_end()));

    extern int end;

    pmm_claim_area(0, arch_vmm_v2p((uintptr_t)&end, ARCH_VMM_AREA_KERNEL));
    pmm_apply_memory_map(false, MIN(PMM_PREALLOCATED_END, pmm_managed_end()));

    // Allocate and install bitmaps for memory above the preallocated range.

    for (size_t i = PML1_PREALLOCATED_BITMAPS; i < PML2_MAX_ENTRIES && pmm_region_capacity(i); i++) {

        uintptr_t phys = pmm_alloc_block();

        if (unlikely(phys == PMM_INVALID_ADDRESS))
            kpanicf("pmm: failed to allocate bitmap for PML2 entry %ld\n", i);

        uintptr_t virt = arch_vmm_p2v(phys, ARCH_VMM_AREA_HEAP);

        if (unlikely(virt == PMM_INVALID_ADDRESS))
            kpanicf("pmm: failed to map bitmap for PML2 entry %ld\n", i);

        pmm_install_bitmap(i, virt);
    }

    // Replay the complete map now that every managed bitmap exists.

    pmm_apply_memory_map(true, pmm_managed_end());

    pmm_claim_area(0, arch_vmm_v2p((uintptr_t)&end, ARCH_VMM_AREA_KERNEL));
    pmm_apply_memory_map(false, pmm_managed_end());

    // Bitmap backing pages live in available RAM and must remain reserved.

    for (size_t i = PML1_PREALLOCATED_BITMAPS; i < PML2_MAX_ENTRIES && pmm_region_capacity(i); i++)
        pmm_claim_area(arch_vmm_v2p(pml2_bitmap[i], ARCH_VMM_AREA_HEAP), PML1_PAGESIZE);

#if defined(DEBUG)
    scoped_lock(&pmm_lock) {
        pmm_validate_locked();
    }
#endif

#if DEBUG_LEVEL_INFO
    kprintf("pmm: physical memory: %ld KB\n", pmm_max_memory / 1024);
#endif
}


TEST(pmm_small_alloc_test, {
    uint64_t used = pmm_get_used_memory();

    uintptr_t b1 = pmm_alloc_block();
    uintptr_t b2 = pmm_alloc_block();
    uintptr_t b3 = pmm_alloc_block();
    uintptr_t b4 = pmm_alloc_block();

    DEBUG_ASSERT(b1 != PMM_INVALID_ADDRESS);
    DEBUG_ASSERT(b2 != PMM_INVALID_ADDRESS);
    DEBUG_ASSERT(b3 != PMM_INVALID_ADDRESS);
    DEBUG_ASSERT(b4 != PMM_INVALID_ADDRESS);
    DEBUG_ASSERT(b1 + PML1_PAGESIZE <= pmm_max_memory);
    DEBUG_ASSERT(b2 + PML1_PAGESIZE <= pmm_max_memory);
    DEBUG_ASSERT(b3 + PML1_PAGESIZE <= pmm_max_memory);
    DEBUG_ASSERT(b4 + PML1_PAGESIZE <= pmm_max_memory);
    DEBUG_ASSERT(pmm_get_used_memory() == used + (4 * PML1_PAGESIZE));

    pmm_claim_area(b1, PML1_PAGESIZE);
    DEBUG_ASSERT(pmm_get_used_memory() == used + (4 * PML1_PAGESIZE));

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

    DEBUG_ASSERT(pmm_get_used_memory() == used);
});


TEST(pmm_contiguous_alloc_test, {
    uint64_t used    = pmm_get_used_memory();
    uintptr_t blocks = pmm_alloc_blocks(4);

    DEBUG_ASSERT(blocks != PMM_INVALID_ADDRESS);
    DEBUG_ASSERT(blocks + (4 * PML1_PAGESIZE) <= pmm_max_memory);
    DEBUG_ASSERT(pmm_get_used_memory() == used + (4 * PML1_PAGESIZE));

    uintptr_t next = pmm_alloc_block();

    DEBUG_ASSERT(next != PMM_INVALID_ADDRESS);
    DEBUG_ASSERT(next < blocks || next >= blocks + (4 * PML1_PAGESIZE));

    pmm_free_block(next);
    pmm_free_blocks(blocks, 4);

    DEBUG_ASSERT(pmm_get_used_memory() == used);
});


TEST(pmm_aligned_alloc_test, {
    uint64_t used     = pmm_get_used_memory();
    uintptr_t aligned = pmm_alloc_blocks_aligned(2, PML1_PAGESIZE * 8);

    DEBUG_ASSERT(aligned != PMM_INVALID_ADDRESS);
    DEBUG_ASSERT((aligned & ((PML1_PAGESIZE * 8) - 1)) == 0);
    DEBUG_ASSERT(aligned + (2 * PML1_PAGESIZE) <= pmm_max_memory);

    pmm_free_blocks(aligned, 2);

    DEBUG_ASSERT(pmm_get_used_memory() == used);
});
