/*
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
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

#ifndef _APLUS_X86_VMM_H
#define _APLUS_X86_VMM_H

#ifndef __ASSEMBLY__

    #include <aplus.h>
    #include <aplus/debug.h>
    #include <arch/x86/intr.h>



    #define X86_MMU_PG_P   (1ULL << 0)
    #define X86_MMU_PG_RW  (1ULL << 1)
    #define X86_MMU_PG_U   (1ULL << 2)
    #define X86_MMU_PG_WT  (1ULL << 3)
    #define X86_MMU_PG_CD  (1ULL << 4)
    #define X86_MMU_PG_PS  (1ULL << 7)
    #define X86_MMU_PG_G   (1ULL << 8)
    #define X86_MMU_PG_PAT (1ULL << 12)

    #define X86_MMU_PT_PAT     (1ULL << 7)
    #define X86_MMU_PT_NX      (1ULL << 63)
    #define X86_MMU_PT_ENTRIES (512)


    /* System defined 11-9 */
    #define X86_MMU_PG_AP_PFB (1ULL << 9)
    #define X86_MMU_PT_AP_PFB (1ULL << 9)

    #define X86_MMU_PG_AP_TP_PAGE (0ULL << 10)
    #define X86_MMU_PG_AP_TP_MMAP (1ULL << 10)
    #define X86_MMU_PG_AP_TP_COW  (2ULL << 10)

    #define X86_MMU_PG_AP_TP_MASK (3ULL << 10)


    /* System defined 62-52: remembers whether the page was writable before it was marked COW,
       so that resolving the fault restores the original permission instead of granting RW. */
    #define X86_MMU_PG_AP_COW_RW (1ULL << 52)


    #define X86_PF_P   (1ULL << 0)
    #define X86_PF_W   (1ULL << 1)
    #define X86_PF_U   (1ULL << 2)
    #define X86_PF_R   (1ULL << 3)
    #define X86_PF_I   (1ULL << 4)
    #define X86_PF_PK  (1ULL << 5)
    #define X86_PF_SS  (1ULL << 6)
    #define X86_PF_SGX (1ULL << 15)



    #define X86_MMU_PAGESIZE          0x1000
    #define X86_MMU_HUGE_2MB_PAGESIZE 0x200000
    #define X86_MMU_HUGE_1GB_PAGESIZE 0x40000000

    #define X86_MMU_CLEAR             0x0000000000000000ULL

    /* Sentinel returned by __try_alloc_frame() when physical memory is exhausted. */
    #define X86_MMU_FRAME_NONE        ((uintptr_t)-1ULL)

    #define X86_MMU_DIRTY_ACCESS_MASK 0x0000000000000F9FULL
    #define X86_MMU_ADDRESS_MASK      0x0000FFFFFFFFF000ULL


/* First address above the canonical low half, i.e. the end of userspace.
   This was previously spelled as the decimal literal 800000000000 (~745GiB) in vm_map.c,
   which lands in the middle of the user mmap window. */
    #if defined(__x86_64__)
        #define X86_MMU_USERSPACE_END 0x0000800000000000ULL
    #elif defined(__i386__)
        #define X86_MMU_USERSPACE_END 0xC0000000UL
    #endif


    #define X86_MMU_KERNEL (X86_MMU_PG_P | X86_MMU_PG_RW)

    #define X86_MMU_USER (X86_MMU_PG_P | X86_MMU_PG_RW | X86_MMU_PG_U)



    #if defined(__x86_64__)
typedef uint64_t x86_page_t;
    #elif defined(__i386__)
typedef uint32_t x86_page_t;
    #else
        #error "unsupported architecture"
    #endif

__BEGIN_DECLS


/*!
 * @brief __try_alloc_frame().
 *        Allocate a physical frame, returning X86_MMU_FRAME_NONE when memory is exhausted.
 *
 * Prefer this over __alloc_frame() on any path reachable from userspace: __alloc_frame()
 * panics, which would let an oversized mmap(2) take down the kernel.
 */
static inline uintptr_t __try_alloc_frame(uintptr_t pagesize, bool zero) {

    DEBUG_ASSERT(pagesize);
    DEBUG_ASSERT(X86_MMU_PAGESIZE == PML1_PAGESIZE);


    uintptr_t p;

    if (likely(pagesize == X86_MMU_PAGESIZE)) {
        p = pmm_alloc_block();
    } else {
        p = pmm_alloc_blocks_aligned(pagesize >> 12, pagesize);
    }

    if (unlikely(p == (uintptr_t)-1ULL))
        return X86_MMU_FRAME_NONE;


    if (likely(zero)) {
        memset((void*)arch_vmm_p2v(p, ARCH_VMM_AREA_HEAP), 0, pagesize);
    }

    return p;
}


/*!
 * @brief __alloc_frame().
 *        Allocate a physical frame, panicking when memory is exhausted.
 *
 * Only for boot-time and other paths that genuinely cannot recover.
 */
static inline uintptr_t __alloc_frame(uintptr_t pagesize, bool zero) {

    uintptr_t p = __try_alloc_frame(pagesize, zero);

    if (unlikely(p == X86_MMU_FRAME_NONE))
        kpanicf("vmm: out of physical memory allocating a %ld-byte frame\n", pagesize);

    return p;
}


static inline void __free_frame(uintptr_t p, uintptr_t pagesize) {

    DEBUG_ASSERT(pagesize);
    DEBUG_ASSERT(X86_MMU_PAGESIZE == PML1_PAGESIZE);


    if (likely(pagesize == X86_MMU_PAGESIZE)) {
        pmm_free_block(p);
    } else {
        pmm_free_blocks(p, pagesize >> 12);
    }
}


/* x86_vmm_walk() control flags */
    #define X86_VMM_WALK_CREATE (1 << 0) /* allocate missing intermediate tables */

/* Page size requested from x86_vmm_walk(): stop wherever the existing tables do. */
    #define X86_MMU_WALK_ANY (0UL)

x86_page_t* x86_vmm_walk(uintptr_t pm, uintptr_t virtaddr, uintptr_t* pagesize, uint64_t table_flags, int walk_flags, uint64_t* effective);

void arch_vmm_flush(vmm_address_space_t*, uintptr_t) __nonnull(1);
void arch_vmm_flush_range(vmm_address_space_t*, uintptr_t, size_t) __nonnull(1);
void arch_vmm_flush_all(vmm_address_space_t*) __nonnull(1);

int x86_vmm_resolve(uintptr_t pm, uintptr_t virtaddr, uint64_t err, const char** reason);

int pagefault_handle(interrupt_frame_t*, uintptr_t);

__END_DECLS

#endif
#endif
