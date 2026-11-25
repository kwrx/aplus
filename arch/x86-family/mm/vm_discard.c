/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2026 Antonino Natale
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



__nonnull(1) uintptr_t arch_vmm_discard(vmm_address_space_t* space, uintptr_t virtaddr, size_t length) {

    if (unlikely(length == 0)) {
        return virtaddr;
    }


    uintptr_t s = virtaddr & ~(X86_MMU_PAGESIZE - 1);
    uintptr_t e = virtaddr + length;

    if (unlikely(e < virtaddr)) {
        return ARCH_VMM_MAP_FAILED;
    }

    if (e & (X86_MMU_PAGESIZE - 1)) {
        e = (e & ~(X86_MMU_PAGESIZE - 1)) + X86_MMU_PAGESIZE;
    }


    vmm_tlb_batch_t* batches = NULL;

    spinlock_lock(&space->lock);

    for (; s < e; s += X86_MMU_PAGESIZE) {

        uintptr_t pagesize = X86_MMU_WALK_ANY;

        x86_page_t* d = x86_vmm_walk(space->pm, s, &pagesize, 0, 0, NULL);

        if (!d || pagesize != X86_MMU_PAGESIZE) {
            continue;
        }

        if (!(*d & X86_MMU_PG_AP_PFB) || !(*d & X86_MMU_PG_U) || (*d & X86_MMU_PG_AP_LAZY)) {
            continue;
        }

        if ((*d & X86_MMU_PG_AP_TP_MASK) != X86_MMU_PG_AP_TP_PAGE) {
            continue;
        }


        x86_vmm_tlb_collect(&batches, *d & X86_MMU_ADDRESS_MASK, X86_MMU_PAGESIZE);

        const uint64_t keep = X86_MMU_PG_RW | X86_MMU_PG_U | X86_MMU_PG_WT | X86_MMU_PG_CD | X86_MMU_PT_PAT | X86_MMU_PT_NX;

        *d = X86_MMU_PG_AP_LAZY | (*d & keep) | ((*d & X86_MMU_PG_P) ? 0 : X86_MMU_PG_AP_NONE);

        arch_vmm_flush(space, s);

        if (space->size > 0) {
            space->size--;
        }
    }

    const uint64_t gen = x86_vmm_tlb_bump(space);

    spinlock_unlock(&space->lock);


    x86_vmm_tlb_retire(space, batches, gen);

    return virtaddr;
}
