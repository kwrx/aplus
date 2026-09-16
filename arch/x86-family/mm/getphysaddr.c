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


/**
 * @brief Translates a virtual address in an address space to a physical one.
 *
 * Only a mapped PAGE or SHARED entry names a frame; anything not yet materialised reports failure.
 *
 * @param space The address space to translate in.
 * @param virtaddr The virtual address to translate.
 * @return The physical address, or ARCH_VMM_MAP_FAILED when @p virtaddr is not mapped.
 */
__nonnull(1) uintptr_t arch_vmm_getphysaddr(vmm_address_space_t* space, uintptr_t virtaddr) {


    uintptr_t pagesize = X86_MMU_WALK_ANY;

    uintptr_t s = virtaddr & ~(X86_MMU_PAGESIZE - 1);
    uintptr_t e = ARCH_VMM_MAP_FAILED;


    spinlock_lock(&space->lock);


    {
        x86_page_t* d = x86_vmm_walk(space->pm, s, &pagesize, 0, 0, NULL);

        if (likely(d && *d != X86_MMU_CLEAR)) {

            const uint64_t type = *d & X86_MMU_PG_AP_TP_MASK;

            if (likely(type == X86_MMU_PG_AP_TP_PAGE || type == X86_MMU_PG_AP_TP_SHARED)) {

                e = (*d & X86_MMU_ADDRESS_MASK) + (virtaddr & (pagesize - 1));
            }
        }
    }


    spinlock_unlock(&space->lock);

    return e;
}
