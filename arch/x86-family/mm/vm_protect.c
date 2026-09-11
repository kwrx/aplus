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
 * @brief arch_vmm_mprotect().
 *        Protect virtual memory.
 *
 * @param space: address space.
 * @param virtaddr: virtual address.
 * @param length: size of virtual space.
 * @param flags: @see include/arch/x86/vmm.h
 *
 * @return the base virtual address on success, ARCH_VMM_MAP_FAILED if any page in the
 *         range is not mapped.
 */
__nonnull(1) uintptr_t arch_vmm_mprotect(vmm_address_space_t* space, uintptr_t virtaddr, size_t length, int flags) {

    DEBUG_ASSERT(length > 0);

    if (unlikely(length == 0))
        return ARCH_VMM_MAP_FAILED;


    uintptr_t pagesize;

    uintptr_t s = virtaddr;
    uintptr_t e = virtaddr + length;


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

    if (e & (pagesize - 1))
        e = (e & ~(pagesize - 1)) + pagesize;



    uint64_t b = X86_MMU_PG_P;


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


    bool failed = false;


    spinlock_lock(&space->lock);

    for (; s < e; s += pagesize) {

        pagesize = X86_MMU_WALK_ANY;

        x86_page_t* d = x86_vmm_walk(space->pm, s, &pagesize, 0, 0, NULL);

        if (unlikely(!d || *d == X86_MMU_CLEAR)) {
            pagesize = X86_MMU_PAGESIZE;
            failed   = true;
            break;
        }


        /* Page Table */
        {
            /* Per-page copy. `b` used to be narrowed in place here, so a single absent page
               in the range cleared the present bit for every page after it. */
            uint64_t pb = b;

            if (!(*d & X86_MMU_PG_P))
                pb &= ~X86_MMU_PG_P;

            if (pb & X86_MMU_PG_RW)
                pb |= X86_MMU_PG_AP_COW_RW;


            /* Preserve the software bits that describe the frame rather than its protection:
               PG_AP_PFB records that the frame is ours to free, and dropping it here leaked
               every frame that had ever been through mprotect -- including each PT_LOAD
               segment, which execve() mprotects on every exec. */
            *d = (*d & X86_MMU_ADDRESS_MASK) | (*d & X86_MMU_PG_AP_TP_MASK) | (*d & X86_MMU_PG_AP_PFB) | pb;



#if DEBUG_LEVEL_TRACE
            // kprintf("arch_vmm_mprotect(): virtaddr(%p) physaddr(%p) flags(%p)\n", s, *d & X86_MMU_ADDRESS_MASK, *d & ~X86_MMU_ADDRESS_MASK);
#endif
        }


        arch_vmm_flush(space, s);
    }

    spinlock_unlock(&space->lock);


    if (unlikely(failed))
        return ARCH_VMM_MAP_FAILED;

    return virtaddr;
}
