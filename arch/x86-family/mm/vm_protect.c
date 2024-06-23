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
 */
__nonnull(1) uintptr_t arch_vmm_mprotect(vmm_address_space_t* space, uintptr_t virtaddr, size_t length, int flags) {

    DEBUG_ASSERT(length > 0);


    uintptr_t pagesize;

    uintptr_t s = virtaddr;
    uintptr_t e = virtaddr + length;


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



    spinlock_lock(&space->lock);

    for (; s < e; s += pagesize) {

        x86_page_t* d;

#if defined(__x86_64__)

        /* CR3-L4 */
        { d = &((x86_page_t*)arch_vmm_p2v(space->pm, ARCH_VMM_AREA_HEAP))[(s >> 39) & 0x1FF]; }

        /* PML4-L3 */
        {
            DEBUG_ASSERT((*d != X86_MMU_CLEAR) && "PML4-L3 not exist");

            d = &((x86_page_t*)arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP))[(s >> 30) & 0x1FF];
        }


        /* HUGE_1GB */
        if (!(*d & X86_MMU_PG_PS)) {

            /* PDP-L2 */
            {
                DEBUG_ASSERT((*d != X86_MMU_CLEAR) && "PDP-L2 not exist");

                d = &((x86_page_t*)arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP))[(s >> 21) & 0x1FF];
            }

            /* HUGE_2MB */
            if (!(*d & X86_MMU_PG_PS)) {

                /* PD-L1 */
                {
                    DEBUG_ASSERT((*d != X86_MMU_CLEAR) && "PDT-L1 not exist");

                    d = &((x86_page_t*)arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP))[(s >> 12) & 0x1FF];
                }

                pagesize = X86_MMU_PAGESIZE;

            } else
                pagesize = X86_MMU_HUGE_2MB_PAGESIZE;

        } else
            pagesize = X86_MMU_HUGE_1GB_PAGESIZE;


#elif defined(__i386__)

        /* CR3-L2 */
        { d = &((x86_page_t*)arch_vmm_p2v(space->pm, ARCH_VMM_AREA_HEAP))[(s >> 22) & 0x3FF]; }


        /* HUGE_4MB */
        if (!(*d & X86_MMU_PG_PS)) {

            /* PD-L1 */
            {
                DEBUG_ASSERT((*d != X86_MMU_CLEAR) && "PDT-L1 not exist");

                d = &((x86_page_t*)arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP))[(s >> 12) & 0x3FF];
            }

            pagesize = X86_MMU_PAGESIZE;

        } else
            pagesize = X86_MMU_HUGE_2MB_PAGESIZE;

#endif

        /* Page Table */
        {
            DEBUG_ASSERT((*d != X86_MMU_CLEAR) && "Page unmapped");


            if (!(*d & X86_MMU_PG_P))
                b &= ~X86_MMU_PG_P;

            *d = (*d & X86_MMU_ADDRESS_MASK) | (*d & X86_MMU_PG_AP_TP_MASK) | b;



#if DEBUG_LEVEL_TRACE
            // kprintf("arch_vmm_mprotect(): virtaddr(%p) physaddr(%p) flags(%p)\n", s, *d & X86_MMU_ADDRESS_MASK, *d & ~X86_MMU_ADDRESS_MASK);
#endif
        }


        __asm__ __volatile__("invlpg (%0)" ::"r"(s) : "memory");
    }

    spinlock_unlock(&space->lock);

    return virtaddr;
}
