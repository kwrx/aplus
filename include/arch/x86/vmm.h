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
    #define X86_MMU_DIRTY_ACCESS_MASK 0x0000000000000F9FULL
    #define X86_MMU_ADDRESS_MASK      0x0000FFFFFFFFF000ULL


    #define X86_MMU_KERNEL (X86_MMU_PG_P | X86_MMU_PG_RW)

    #define X86_MMU_USER (X86_MMU_PG_P | X86_MMU_PG_RW | X86_MMU_PG_U)



    #if defined(__x86_64__)
typedef uint64_t x86_page_t;
    #elif
typedef uint32_t x86_page_t;
    #endif

__BEGIN_DECLS


static inline uintptr_t __alloc_frame(uintptr_t pagesize, bool zero) {

    DEBUG_ASSERT(pagesize);
    DEBUG_ASSERT(X86_MMU_PAGESIZE == PML1_PAGESIZE);


    uintptr_t p;

    if (likely(pagesize == X86_MMU_PAGESIZE)) {
        p = pmm_alloc_block();
    } else {
        p = pmm_alloc_blocks_aligned(pagesize >> 12, pagesize);
    }

    DEBUG_ASSERT(p != -1);


    if (likely(zero)) {
        memset((void*)arch_vmm_p2v(p, ARCH_VMM_AREA_HEAP), 0, pagesize);
    }

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


void pagefault_handle(interrupt_frame_t*, uintptr_t);

__END_DECLS

#endif
#endif
