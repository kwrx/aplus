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
 * @brief arch_vmm_lock().
 *        Make a user range safe for the kernel to dereference.
 *
 * Materialises any copy-on-write or demand-paged entry in the range, so that the
 * uio_* accessors can translate it, and opens the SMAP window.
 *
 * Must be paired with arch_vmm_unlock() on the same CPU. The pair is not re-entrant:
 * a nested unlock closes the window for the outer region too.
 *
 * @param space: address space owning the range.
 * @param virtaddr: base of the range.
 * @param size: length of the range in bytes.
 */
__nonnull(1) void arch_vmm_lock(vmm_address_space_t* space, uintptr_t virtaddr, size_t size) {

    DEBUG_ASSERT(space->pm);
    DEBUG_ASSERT(size > 0);


    /* Pre-fault the range. arch_vmm_getphysaddr() deliberately refuses to resolve a
       not-yet-materialised entry -- it would have to fault it into whichever address space
       is loaded in CR3, which need not be @space -- so the range has to be settled here,
       before the kernel starts dereferencing it. */
    if (likely(size > 0)) {

        const uintptr_t s = virtaddr & ~(X86_MMU_PAGESIZE - 1);
        const uintptr_t e = (virtaddr + size + X86_MMU_PAGESIZE - 1) & ~(X86_MMU_PAGESIZE - 1);

        /* Only meaningful for the address space we are actually running on: x86_vmm_resolve()
           walks a root table, and a range belonging to a space that is not loaded cannot be
           touched through uio_* anyway. */
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
    /* stac/clac rather than a CR4 read-modify-write: toggling CR4.SMAP is a serializing
       operation on every user-memory access the kernel makes. */
    if (cpu_has(current_cpu->id, X86_FEATURE_SMAP))
        __asm__ __volatile__("stac" ::: "cc");
#endif
}
