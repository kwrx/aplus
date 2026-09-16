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
 * @brief Invalidates the TLB entry for a single page of an address space, on every CPU that holds it.
 *
 * @param space Address space the entry belongs to.
 * @param virtaddr Address whose translation is now stale.
 */
__nonnull(1) void arch_vmm_flush(vmm_address_space_t* space, uintptr_t virtaddr) {

    if (space->pm == x86_get_cr3() || virtaddr >= X86_MMU_USERSPACE_END) {
        __asm__ __volatile__("invlpg (%0)" ::"r"(virtaddr) : "memory");
    }

    /* TODO: cross-CPU shootdown. Until an IPI vector exists, another CPU running this same
       address space keeps its stale entry. Tracked as phase 5.1 of the VMM remediation. */
}


/**
 * @brief Invalidates the TLB entries covering a range, falling back to a full flush for a large one.
 *
 * @param space Address space the range belongs to.
 * @param virtaddr Base address.
 * @param length Size of the range in bytes.
 */
__nonnull(1) void arch_vmm_flush_range(vmm_address_space_t* space, uintptr_t virtaddr, size_t length) {

    if (unlikely(length == 0))
        return;


    const uintptr_t s = virtaddr & ~(X86_MMU_PAGESIZE - 1);
    const uintptr_t e = (virtaddr + length + X86_MMU_PAGESIZE - 1) & ~(X86_MMU_PAGESIZE - 1);

    if ((e - s) >> 12 > 64) {
        return arch_vmm_flush_all(space);
    }

    for (uintptr_t p = s; p < e; p += X86_MMU_PAGESIZE)
        arch_vmm_flush(space, p);
}


/**
 * @brief Drops every non-global TLB entry for an address space.
 *
 * @param space Address space to flush.
 */
__nonnull(1) void arch_vmm_flush_all(vmm_address_space_t* space) {

    if (space->pm == x86_get_cr3())
        x86_set_cr3(x86_get_cr3());

    /* TODO: cross-CPU shootdown, as above. */
}
