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
 * @brief Makes a user range safe for the kernel to dereference, and opens the SMAP window.
 *
 * Must be paired with arch_vmm_unlock() on the same CPU, and the pair is not re-entrant.
 *
 * @param space Address space owning the range.
 * @param virtaddr Base of the range.
 * @param size Length of the range in bytes.
 */
__nonnull(1) void arch_vmm_lock(vmm_address_space_t* space, uintptr_t virtaddr, size_t size) {

    DEBUG_ASSERT(space->pm);
    DEBUG_ASSERT(size > 0);


    if (likely(size > 0)) {

        const uintptr_t s = virtaddr & ~(X86_MMU_PAGESIZE - 1);
        const uintptr_t e = (virtaddr + size + X86_MMU_PAGESIZE - 1) & ~(X86_MMU_PAGESIZE - 1);

        if (space->pm == x86_get_cr3()) {

            scoped_lock(&space->lock) {

                for (uintptr_t p = s; p < e && p >= s; p += X86_MMU_PAGESIZE) {

                    if (x86_vmm_resolve(space->pm, p, X86_PF_U | X86_PF_W, NULL) == 0)
                        arch_vmm_flush(space, p);
                }
            }
        }
    }


#if defined(CONFIG_X86_ENABLE_SMAP)
    if (cpu_has(current_cpu->id, X86_FEATURE_SMAP))
        __asm__ __volatile__("stac" ::: "cc");
#endif
}
