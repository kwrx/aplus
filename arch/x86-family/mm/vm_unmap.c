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
 * @brief arch_vmm_unmap().
 *        Unmap virtual memory.
 *
 * @param space: address space.
 * @param virtaddr: virtual address.
 * @param length: size of virtual space.
 */
__nonnull(1) uintptr_t arch_vmm_unmap(vmm_address_space_t* space, uintptr_t virtaddr, size_t length) {

    DEBUG_ASSERT(length > 0);

    if (unlikely(length == 0))
        return virtaddr;


    uintptr_t pagesize = X86_MMU_PAGESIZE;

    uintptr_t s = virtaddr;
    uintptr_t e = virtaddr + length;


    if (unlikely(e < virtaddr))
        return virtaddr;


    if (s & (X86_MMU_PAGESIZE - 1))
        s = (s & ~(X86_MMU_PAGESIZE - 1));

    if (e & (X86_MMU_PAGESIZE - 1))
        e = (e & ~(X86_MMU_PAGESIZE - 1)) + X86_MMU_PAGESIZE;



    spinlock_lock(&space->lock);

    for (; s < e; s += pagesize) {

        pagesize = X86_MMU_WALK_ANY;

        x86_page_t* d = x86_vmm_walk(space->pm, s, &pagesize, 0, 0, NULL);

        if (unlikely(!d)) {

            pagesize = X86_MMU_PAGESIZE;
            continue;
        }


        /* Page Table */
        {
            if (unlikely(*d == X86_MMU_CLEAR)) {
                pagesize = X86_MMU_PAGESIZE;
                continue;
            }

            if (*d & X86_MMU_PG_AP_PFB)
                __free_frame(*d & X86_MMU_ADDRESS_MASK, pagesize);


#if DEBUG_LEVEL_TRACE
                // kprintf("arch_vmm_unmap(): virtaddr(%p) physaddr(%p) pagesize(%p)\n", s, *d & X86_MMU_ADDRESS_MASK, pagesize);
#endif

            *d = X86_MMU_CLEAR;
        }


        arch_vmm_flush(space, s);

        if (space->size >= (pagesize >> 12))
            space->size -= pagesize >> 12;
        else
            space->size = 0;
    }

    spinlock_unlock(&space->lock);

    return virtaddr;
}
