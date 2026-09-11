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
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>
#include <arch/x86/vmm.h>



/*!
 * @brief arch_vmm_map().
 *        Map virtual memory.
 *
 * @param space: address space.
 * @param virtaddr: virtual address.
 * @param physaddr: physical address.
 * @param length: size of virtual space.
 * @param flags: @see include/arch/x86/vmm.h
 *
 * @return the base virtual address on success, ARCH_VMM_MAP_FAILED on failure.
 *         On failure nothing of this request stays mapped: every page this call
 *         established is rolled back before returning.
 */
__nonnull(1) uintptr_t arch_vmm_map(vmm_address_space_t* space, uintptr_t virtaddr, uintptr_t physaddr, size_t length, int flags) {

    DEBUG_ASSERT(length > 0);

    if (unlikely(length == 0))
        return ARCH_VMM_MAP_FAILED;


    uintptr_t pagesize;

    uintptr_t s = virtaddr;
    uintptr_t p = physaddr;
    uintptr_t e = virtaddr + length;


    /* Reject a length that wraps: the loop below would never terminate. */
    if (unlikely(e < virtaddr))
        return ARCH_VMM_MAP_FAILED;


    if (flags & ARCH_VMM_MAP_HUGETLB) {

#if defined(__x86_64__)
        if (flags & ARCH_VMM_MAP_HUGE_1GB)
            pagesize = X86_MMU_HUGE_1GB_PAGESIZE;
        else
#endif
            pagesize = X86_MMU_HUGE_2MB_PAGESIZE;

    } else
        pagesize = X86_MMU_PAGESIZE;



    if (s & (pagesize - 1))
        s = (s & ~(pagesize - 1));

    if (p & (pagesize - 1))
        p = (p & ~(pagesize - 1));

    if (e & (pagesize - 1))
        e = (e & ~(pagesize - 1)) + pagesize;


    const uintptr_t base = s;


    uint64_t b = X86_MMU_PG_P; // flags for Page Table



    // * Prepare FLAGS for the Page Table

    if (flags & ARCH_VMM_MAP_DISABLED)
        b &= ~X86_MMU_PG_P;

    if (flags & ARCH_VMM_MAP_RDWR)
        b |= X86_MMU_PG_RW;

    if (flags & ARCH_VMM_MAP_USER)
        b |= X86_MMU_PG_U;

    if (flags & ARCH_VMM_MAP_WRITE_THROUGH)
        b |= X86_MMU_PG_WT;

    if (flags & ARCH_VMM_MAP_UNCACHED)
        b |= X86_MMU_PG_CD;

    if (flags & ARCH_VMM_MAP_SHARED)
        b |= X86_MMU_PG_G;



#if DEBUG_LEVEL_TRACE
    if (flags & ARCH_VMM_MAP_DEMAND)
        DEBUG_ASSERT((flags & ARCH_VMM_MAP_TYPE_MASK) == ARCH_VMM_MAP_TYPE_PAGE && "Only TYPE_PAGE can be no-prefault");
#endif

    //* Set Page Type
    switch ((flags & ARCH_VMM_MAP_TYPE_MASK)) {

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
    if (flags & ARCH_VMM_MAP_NOEXEC)
        if (boot_cpu_has(X86_FEATURE_NX))
            b |= X86_MMU_PT_NX; /* NX */

#endif

    if (flags & ARCH_VMM_MAP_HUGETLB) {

        b |= X86_MMU_PG_PS;

        if (flags & ARCH_VMM_MAP_VIDEO_MEMORY)
            if (boot_cpu_has(X86_FEATURE_PAT))
                b |= X86_MMU_PG_PAT; /* WC */

    } else {

        if (flags & ARCH_VMM_MAP_VIDEO_MEMORY)
            if (boot_cpu_has(X86_FEATURE_PAT))
                b |= X86_MMU_PT_PAT; /* WC */
    }


    /* A demand mapping starts out absent and is faulted in by pagefault_handle(). Remember
       whether the caller asked for a writable page so that resolving the fault restores the
       requested permission rather than unconditionally granting write access. */
    if (b & X86_MMU_PG_RW)
        b |= X86_MMU_PG_AP_COW_RW;


    bool failed = false;


    spinlock_lock(&space->lock);

    for (; s < e; s += pagesize, p += pagesize) {

        /* Flags for intermediate tables. Computed per page rather than once for the whole
           request: a range may straddle the user/kernel split, and the hardware ANDs the user
           bit across every level, so a table shared with the wrong half would be unusable. */
        uint64_t q = X86_MMU_PG_P | X86_MMU_PG_RW;

        if (s < X86_MMU_USERSPACE_END)
            q |= X86_MMU_PG_U;


        uintptr_t level = pagesize;

        x86_page_t* d = x86_vmm_walk(space->pm, s, &level, q, X86_VMM_WALK_CREATE, NULL);

        if (unlikely(!d)) {
            failed = true;
            break;
        }


        /* Page Table */
        {
            /* Checked unconditionally: this used to be a DEBUG_ASSERT, which compiles away in a
               release build and let an overlapping map silently overwrite a live entry and leak
               its frame. */
            if (unlikely(*d != X86_MMU_CLEAR)) {
                failed = true;
                break;
            }


            if (flags & ARCH_VMM_MAP_FIXED) {

                *d = p | b;

            } else {

                if (flags & ARCH_VMM_MAP_DEMAND) {

                    *d = X86_MMU_PG_AP_TP_COW | (b & ~X86_MMU_PG_P);

                } else {

                    /* Zeroed, not merely allocated. These frames back anonymous mmap(2) and
                       brk(2) memory, and handing them over as-is disclosed whatever the
                       previous owner had left in them. */
                    uintptr_t frame = __try_alloc_frame(pagesize, true);

                    if (unlikely(frame == X86_MMU_FRAME_NONE)) {
                        failed = true;
                        break;
                    }

                    *d = frame | X86_MMU_PG_AP_PFB | b;
                }
            }
        }


        arch_vmm_flush(space, s);

        space->size += pagesize >> 12;
    }

    spinlock_unlock(&space->lock);


    if (unlikely(failed)) {

        /* Roll back the pages this call established. Intermediate tables allocated along the
           way are left in place: they are valid empty tables, will be reused by a later map at
           the same address, and are released with the rest of the hierarchy when the address
           space is torn down. */
        if (s > base)
            arch_vmm_unmap(space, base, s - base);

        return ARCH_VMM_MAP_FAILED;
    }

    return virtaddr;
}
