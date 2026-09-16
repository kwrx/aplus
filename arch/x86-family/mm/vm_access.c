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
 * @brief arch_vmm_access().
 *        Permission check for a virtual address.
 *
 * This backs uio_check() (@see include/aplus/hal.h), which guards every syscall that
 * dereferences a user-supplied pointer, so it is the boundary between userspace and the
 * kernel's own address space. It must be exact.
 *
 * @param space: address space.
 * @param virtaddr: virtual address.
 * @param mode: access mode. R_OK/W_OK/X_OK describe a userspace access and require the
 *              page to be reachable from CPL 3; S_OK instead asserts a supervisor page.
 *
 * @return 0 if the access is permitted, -1 otherwise.
 */
__nonnull(1) int arch_vmm_access(vmm_address_space_t* space, uintptr_t virtaddr, int mode) {


    uintptr_t s = virtaddr;
    int e       = 0;


    if (s & (X86_MMU_PAGESIZE - 1))
        s = (s & ~(X86_MMU_PAGESIZE - 1));


    if (!(mode & (S_OK | K_OK))) {

        if (unlikely(virtaddr >= X86_MMU_USERSPACE_END))
            return -1;
    }


#define check_or_fail(x) \
    if (!(x)) {          \
        e = -1;          \
        goto out;        \
    }


    spinlock_lock(&space->lock);


    {
        uintptr_t pagesize = X86_MMU_WALK_ANY;
        uint64_t effective = 0;

        x86_page_t* d = x86_vmm_walk(space->pm, s, &pagesize, 0, 0, &effective);

        check_or_fail(d);
        check_or_fail(*d != X86_MMU_CLEAR);


        uint64_t perm = (effective & (*d | ~(X86_MMU_PG_U | X86_MMU_PG_RW))) | ((effective | *d) & X86_MMU_PT_NX) | (*d & X86_MMU_PG_P);


        /* Page Table */
        {
            if (!(mode & (S_OK | K_OK))) {
                check_or_fail(perm & X86_MMU_PG_U);
            }

            if (mode & R_OK) {
                if (!(perm & X86_MMU_PG_P)) {
                    check_or_fail((*d & X86_MMU_PG_AP_TP_MASK) == X86_MMU_PG_AP_TP_COW);
                }
            }

#if defined(__x86_64__)
            if (mode & X_OK) {
                check_or_fail(!(perm & X86_MMU_PT_NX));
            }
#endif

            if (mode & W_OK) {
                if (!(perm & X86_MMU_PG_RW)) {
                    check_or_fail((*d & X86_MMU_PG_AP_TP_MASK) == X86_MMU_PG_AP_TP_COW);
                }
            }

            if (mode & S_OK) {
                check_or_fail(!(perm & X86_MMU_PG_U));
            }
        }
    }


out:

    spinlock_unlock(&space->lock);

    return e;
}
