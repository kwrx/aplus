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



#define TABLE(e) ((x86_page_t*)arch_vmm_p2v((uintptr_t)(e) & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP))


/**
 * @brief Walks a page table hierarchy down to the entry mapping an address.
 *
 * The returned pointer aliases the live page tables, so the caller must hold the address space lock.
 *
 * @param pm Physical address of the root table.
 * @param virtaddr Address to translate.
 * @param pagesize In/out. The page size to reach, or X86_MMU_WALK_ANY; receives the size actually mapped.
 * @param table_flags Flags applied to intermediate tables created by this walk.
 * @param walk_flags @see X86_VMM_WALK_*
 * @param effective Optional. Receives the permissions the hardware would apply across the intermediate levels.
 * @return A pointer to the leaf entry, or NULL if the walk could not complete.
 */
x86_page_t* x86_vmm_walk(uintptr_t pm, uintptr_t virtaddr, uintptr_t* pagesize, uint64_t table_flags, int walk_flags, uint64_t* effective) {

    DEBUG_ASSERT(pm);
    DEBUG_ASSERT(pagesize);


    const uintptr_t want = *pagesize;
    const bool create    = !!(walk_flags & X86_VMM_WALK_CREATE);

    uintptr_t s = virtaddr;
    x86_page_t* d;

    uint64_t eff = X86_MMU_PG_U | X86_MMU_PG_RW;

    if (effective)
        *effective = eff;


#define DESCEND(shift, mask)                                                       \
    {                                                                              \
        if (*d == X86_MMU_CLEAR) {                                                 \
                                                                                   \
            if (!create)                                                           \
                return NULL;                                                       \
                                                                                   \
            uintptr_t __t = __try_alloc_frame(X86_MMU_PAGESIZE, true);             \
                                                                                   \
            if (unlikely(__t == X86_MMU_FRAME_NONE))                               \
                return NULL;                                                       \
                                                                                   \
            *d = __t | X86_MMU_PT_AP_PFB | table_flags;                            \
        }                                                                          \
                                                                                   \
        eff &= (*d) | ~(X86_MMU_PG_U | X86_MMU_PG_RW);                             \
        eff |= (*d) & X86_MMU_PT_NX;                                               \
                                                                                   \
        if (effective)                                                             \
            *effective = eff;                                                      \
                                                                                   \
        d = &TABLE(*d)[(s >> (shift)) & (mask)];                                   \
    }


#if defined(__x86_64__)

    d = &((x86_page_t*)arch_vmm_p2v(pm, ARCH_VMM_AREA_HEAP))[(s >> 39) & 0x1FF];

    DESCEND(30, 0x1FF);

    if (*d & X86_MMU_PG_PS) {

        if (want != X86_MMU_WALK_ANY && want != X86_MMU_HUGE_1GB_PAGESIZE)
            return NULL;

        return *pagesize = X86_MMU_HUGE_1GB_PAGESIZE, d;
    }

    if (want == X86_MMU_HUGE_1GB_PAGESIZE)
        return *pagesize = X86_MMU_HUGE_1GB_PAGESIZE, d;


    DESCEND(21, 0x1FF);

    if (*d & X86_MMU_PG_PS) {

        if (want != X86_MMU_WALK_ANY && want != X86_MMU_HUGE_2MB_PAGESIZE)
            return NULL;

        return *pagesize = X86_MMU_HUGE_2MB_PAGESIZE, d;
    }

    if (want == X86_MMU_HUGE_2MB_PAGESIZE)
        return *pagesize = X86_MMU_HUGE_2MB_PAGESIZE, d;


    DESCEND(12, 0x1FF);

#elif defined(__i386__)

    d = &((x86_page_t*)arch_vmm_p2v(pm, ARCH_VMM_AREA_HEAP))[(s >> 22) & 0x3FF];

    if (*d & X86_MMU_PG_PS) {

        if (want != X86_MMU_WALK_ANY && want != X86_MMU_HUGE_2MB_PAGESIZE)
            return NULL;

        return *pagesize = X86_MMU_HUGE_2MB_PAGESIZE, d;
    }

    if (want == X86_MMU_HUGE_2MB_PAGESIZE)
        return *pagesize = X86_MMU_HUGE_2MB_PAGESIZE, d;


    DESCEND(12, 0x3FF);

#else
    #error "Unsupported architecture!"
#endif

#undef DESCEND

    return *pagesize = X86_MMU_PAGESIZE, d;
}
