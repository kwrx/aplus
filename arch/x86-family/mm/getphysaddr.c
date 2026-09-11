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
 * @brief arch_vmm_getphysaddr().
 *        Translate a virtual address in @space to a physical one.
 *
 * @param space: address space.
 * @param virtaddr: virtual address.
 *
 * @return the physical address, or ARCH_VMM_MAP_FAILED when @virtaddr is not mapped.
 *         Zero is a valid physical address, so it cannot be used to signal failure --
 *         which is what the release build used to return for an unmapped address once
 *         its DEBUG_ASSERTs compiled away.
 */
__nonnull(1) uintptr_t arch_vmm_getphysaddr(vmm_address_space_t* space, uintptr_t virtaddr) {


    uintptr_t pagesize = X86_MMU_WALK_ANY;

    uintptr_t s = virtaddr & ~(X86_MMU_PAGESIZE - 1);
    uintptr_t e = ARCH_VMM_MAP_FAILED;


    spinlock_lock(&space->lock);


    {
        x86_page_t* d = x86_vmm_walk(space->pm, s, &pagesize, 0, 0, NULL);

        /* Page Table */
        if (likely(d && *d != X86_MMU_CLEAR)) {

            /* A non-PAGE type is a copy-on-write or file mapping that has not been
               materialised yet, so there is no frame to report. The caller has to touch the
               page (or arch_vmm_lock() it) first; resolving it here used to call
               pagefault_handle(), which walks CR3 rather than @space and would therefore
               fault a page into whichever address space happened to be loaded. */
            if (likely((*d & X86_MMU_PG_AP_TP_MASK) == X86_MMU_PG_AP_TP_PAGE)) {

                e = (*d & X86_MMU_ADDRESS_MASK) + (virtaddr & (pagesize - 1));
            }
        }
    }


    spinlock_unlock(&space->lock);

    return e;
}
