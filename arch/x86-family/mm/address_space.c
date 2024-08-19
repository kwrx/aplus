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

#include <stdatomic.h>
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



static x86_page_t __mm_copy_data(x86_page_t* __s, size_t* size, bool on_demand, int level) {

    DEBUG_ASSERT(__s);
    DEBUG_ASSERT(size);


    uintptr_t pagesize = 0UL;

    switch (level) {

        case 1:
            pagesize = X86_MMU_PAGESIZE;
            break;
        case 2:
            pagesize = X86_MMU_HUGE_2MB_PAGESIZE;
            break;
#if defined(__x86_64__)
        case 3:
            pagesize = X86_MMU_HUGE_1GB_PAGESIZE;
            break;
#endif
        default:
            kpanicf("__mm_copy_data(): PANIC! Page map level too high or invalid: %d\n", level);
    }

    DEBUG_ASSERT(pagesize);


    *size += pagesize;


#if defined(CONFIG_DEMAND_PAGING)
    if (on_demand) {

        return (*__s = (*__s & ~(X86_MMU_PG_RW | X86_MMU_PG_AP_TP_MASK)) | X86_MMU_PG_AP_TP_COW);

    } else
#endif
    {

        uintptr_t page = __alloc_frame(pagesize, false);

        memcpy((void*)arch_vmm_p2v(page, ARCH_VMM_AREA_HEAP), (void*)arch_vmm_p2v(*__s & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP), (size_t)pagesize);


        return page | X86_MMU_PG_AP_PFB | (*__s & ~X86_MMU_ADDRESS_MASK);
    }
}


static void __mm_copy_page(x86_page_t* __s, x86_page_t* __d, size_t* size, int level, int flags) {

    DEBUG_ASSERT(__s);
    DEBUG_ASSERT(__d);
    DEBUG_ASSERT(size);



    if ((*__s & X86_MMU_PG_U) == 0 || (*__s & X86_MMU_PG_G)) {

        *__d = *__s;

    } else {

        if (flags & ARCH_VMM_CLONE_USERSPACE) {


            if ((*__s & X86_MMU_PG_AP_TP_MASK) == X86_MMU_PG_AP_TP_COW) {

                *__d = *__s;

            } else {

                *__d = __mm_copy_data(__s, size, (flags & ARCH_VMM_CLONE_DEMAND), level);
            }
        }
    }
}


static void __mm_copy_table(uintptr_t __s, uintptr_t __d, size_t* size, int level, int flags) {

    DEBUG_ASSERT(__s);
    DEBUG_ASSERT(__d);
    DEBUG_ASSERT(size);


    x86_page_t* s = (x86_page_t*)arch_vmm_p2v(__s, ARCH_VMM_AREA_HEAP);
    x86_page_t* d = (x86_page_t*)arch_vmm_p2v(__d, ARCH_VMM_AREA_HEAP);


    for (size_t i = 0; i < X86_MMU_PT_ENTRIES; i++) {

        if (s[i] == X86_MMU_CLEAR)
            continue;


        if ((s[i] & X86_MMU_PG_PS) || (level == 1)) {

            __mm_copy_page(&s[i], &d[i], size, level, flags);

        } else {

            d[i] = __alloc_frame(X86_MMU_PAGESIZE, true) | (s[i] & ~X86_MMU_ADDRESS_MASK) | X86_MMU_PT_AP_PFB;

            __mm_copy_table(((uintptr_t)s[i]) & X86_MMU_ADDRESS_MASK, ((uintptr_t)d[i]) & X86_MMU_ADDRESS_MASK, size, level - 1, flags);
        }
    }
}



static void __mm_free_data(x86_page_t* __s, int level) {

    DEBUG_ASSERT(__s);


    uintptr_t pagesize = 0UL;

    switch (level) {

        case 1:
            pagesize = X86_MMU_PAGESIZE;
            break;
        case 2:
            pagesize = X86_MMU_HUGE_2MB_PAGESIZE;
            break;
#if defined(__x86_64__)
        case 3:
            pagesize = X86_MMU_HUGE_1GB_PAGESIZE;
            break;
#endif
        default:
            kpanicf("__mm_free_data(): PANIC! Page map level too high or invalid: %d\n", level);
    }

    DEBUG_ASSERT(pagesize);



    if (*__s & X86_MMU_PG_AP_PFB) {

        __free_frame(*__s & X86_MMU_ADDRESS_MASK, pagesize);
    }
}


static void __mm_free_page(x86_page_t* __s, int level) {

    DEBUG_ASSERT(__s);


    if ((*__s & X86_MMU_PG_U) == 0 || (*__s & X86_MMU_PG_G)) {

        return;

    } else {

        if ((*__s & X86_MMU_PG_AP_TP_MASK) == X86_MMU_PG_AP_TP_COW) {

            return;

        } else {

            __mm_free_data(__s, level);
        }
    }
}


static void __mm_free_table(uintptr_t __s, int level) {

    DEBUG_ASSERT(__s);

    x86_page_t* s = (x86_page_t*)arch_vmm_p2v(__s, ARCH_VMM_AREA_HEAP);


    for (size_t i = 0; i < X86_MMU_PT_ENTRIES; i++) {

        if (s[i] == X86_MMU_CLEAR)
            continue;


        if ((s[i] & X86_MMU_PG_PS) || (level == 1)) {

            __mm_free_page(&s[i], level);

        } else {

            __mm_free_table(((uintptr_t)s[i]) & X86_MMU_ADDRESS_MASK, level - 1);

            // FIXME: not safe to free frame here, as it may be used by system page tables
            // // if(s[i] & X86_MMU_PT_AP_PFB) {
            // //     __free_frame(((uintptr_t) s[i]) & X86_MMU_ADDRESS_MASK, X86_MMU_PAGESIZE);
            // // }
        }
    }
}


__returns_nonnull vmm_address_space_t* arch_vmm_create_address_space(vmm_address_space_t* parent, int flags) {

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(parent->pm);

    vmm_address_space_t* dest = (vmm_address_space_t*)kcalloc(1, sizeof(vmm_address_space_t), GFP_KERNEL);

    if (unlikely(!dest)) {
        kpanicf("arch_vmm_create_address_space(): PANIC! Failed to allocate memory for address space!\n");
    }


    dest->pm = __alloc_frame(X86_MMU_PAGESIZE, true);



    size_t size = 0UL;

#if defined(__x86_64__)
    scoped_lock(&parent->lock) {
        __mm_copy_table(parent->pm, dest->pm, &size, 4, flags);
    }
#elif defined(__i386__)
    scoped_lock(&parent->lock) {
        __mm_copy_table(parent->pm, dest->pm, &size, 2, flags);
    }
#else
    #error "Unsupported architecture!"
#endif


    dest->size     = size;
    dest->refcount = 1;


    if (flags & ARCH_VMM_CLONE_USERSPACE) {

        dest->mmap.heap_start = parent->mmap.heap_start;
        dest->mmap.heap_end   = parent->mmap.heap_end;

        memcpy(&dest->mmap.mappings, &parent->mmap.mappings, sizeof(mmap_mapping_t) * CONFIG_MMAP_MAX);

    } else {

        dest->mmap.heap_start = parent->mmap.heap_start;
        dest->mmap.heap_end   = parent->mmap.heap_start;
    }


    spinlock_init_with_flags(&dest->lock, SPINLOCK_FLAGS_CPU_OWNER | SPINLOCK_FLAGS_RECURSIVE);

    return dest;
}


void arch_vmm_free_address_space(vmm_address_space_t* space) {

    DEBUG_ASSERT(space);
    DEBUG_ASSERT(space->pm);

    if (atomic_fetch_sub(&space->refcount, 1) > 1) {
        return;
    }


    // TODO: free all mappings

#if defined(__x86_64__)
    scoped_lock(&space->lock) {
        __mm_free_table(space->pm, 4);
    }
#elif defined(__i386__)
    scoped_lock(&space->lock) {
        __mm_free_table(space->pm, 2);
    }
#else
    #error "Unsupported architecture!"
#endif


    // FIXME: maybe unsafe
    //__free_frame(space->pm, X86_MMU_PAGESIZE);

    space->pm              = 0UL;
    space->size            = 0UL;
    space->mmap.heap_start = 0UL;
    space->mmap.heap_end   = 0UL;
}
