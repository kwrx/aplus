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
#include <aplus/shm.h>
#include <aplus/smp.h>
#include <aplus/task.h>

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>
#include <arch/x86/vmm.h>



/**
 * @brief Reports the page size mapped by a leaf entry at a given paging level.
 *
 * @param level The paging level.
 * @return The page size in bytes.
 */
static uintptr_t __mm_pagesize_for_level(int level) {

    switch (level) {

        case 1:
            return X86_MMU_PAGESIZE;
        case 2:
            return X86_MMU_HUGE_2MB_PAGESIZE;
#if defined(__x86_64__)
        case 3:
            return X86_MMU_HUGE_1GB_PAGESIZE;
#endif
    }

    kpanicf("__mm_pagesize_for_level(): PANIC! Page map level too high or invalid: %d\n", level);
    return 0UL;
}


/**
 * @brief Tells whether a top-level slot describes memory owned by the kernel alone.
 *
 * Such a slot is shared by reference between every address space instead of being copied.
 *
 * @param level The paging level.
 * @param i The slot within that level.
 * @return true if the slot is the kernel's.
 */
static inline bool __mm_is_kernel_slot(int level, size_t i) {

#if defined(__x86_64__)

    #define MM_PML4_SPAN       (1ULL << 39)
    #define MM_PML4_SLOT(addr) (((uintptr_t)(addr) >> 39) & 0x1FF)
    #define MM_PML4_SPANS(sz)  ((((uintptr_t)(sz)) + MM_PML4_SPAN - 1) >> 39)

    if (level != 4)
        return false;

    if (i >= MM_PML4_SLOT(KERNEL_HEAP_AREA) && i < MM_PML4_SLOT(KERNEL_HEAP_AREA) + MM_PML4_SPANS(KERNEL_HEAP_SIZE))
        return true;

    if (i == MM_PML4_SLOT(KERNEL_HIGH_AREA))
        return true;

#else

    (void)level;
    (void)i;

#endif

    return false;
}


static x86_page_t __mm_copy_data(x86_page_t* __s, size_t* size, bool on_demand, int level) {

    DEBUG_ASSERT(__s);
    DEBUG_ASSERT(size);


    uintptr_t pagesize = __mm_pagesize_for_level(level);

    DEBUG_ASSERT(pagesize);


    *size += pagesize >> 12;


#if defined(CONFIG_DEMAND_PAGING)
    if (on_demand) {

        x86_page_t e = (*__s & ~(X86_MMU_PG_RW | X86_MMU_PG_AP_TP_MASK)) | X86_MMU_PG_AP_TP_COW;

        if (*__s & X86_MMU_PG_RW)
            e |= X86_MMU_PG_AP_COW_RW;

        return (*__s = e);

    } else
#endif
    {

        uintptr_t page = __alloc_frame(pagesize, false);

        memcpy((void*)arch_vmm_p2v(page, ARCH_VMM_AREA_HEAP), (void*)arch_vmm_p2v(*__s & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP), (size_t)pagesize);


        return page | X86_MMU_PG_AP_PFB | (*__s & ~X86_MMU_ADDRESS_MASK);
    }
}


/**
 * @brief Gives the destination table its own entry for a leaf the source holds.
 *
 * @param __s The source entry.
 * @param __d The destination entry.
 * @param size Accumulates the memory the copy accounts for.
 * @param level The paging level of the entry.
 * @param flags The ARCH_VMM_CLONE_* flags the clone was asked for.
 */
static void __mm_copy_page(x86_page_t* __s, x86_page_t* __d, size_t* size, int level, int flags) {

    DEBUG_ASSERT(__s);
    DEBUG_ASSERT(__d);
    DEBUG_ASSERT(size);



    if ((*__s & X86_MMU_PG_U) == 0 || (*__s & X86_MMU_PG_G)) {

        *__d = *__s;

    } else {

        if (flags & ARCH_VMM_CLONE_USERSPACE) {


            const uint64_t type = *__s & X86_MMU_PG_AP_TP_MASK;

            if (type == X86_MMU_PG_AP_TP_COW || type == X86_MMU_PG_AP_TP_SHARED) {

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


        if (__mm_is_kernel_slot(level, i)) {

            d[i] = s[i] & ~X86_MMU_PT_AP_PFB;
            continue;
        }


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


    uintptr_t pagesize = __mm_pagesize_for_level(level);

    DEBUG_ASSERT(pagesize);



    if (*__s & X86_MMU_PG_AP_PFB) {

        __free_frame(*__s & X86_MMU_ADDRESS_MASK, pagesize);
    }
}


/**
 * @brief Releases a leaf's frame, if this address space is the one that owns it.
 *
 * @param __s The entry to release.
 * @param level The paging level of the entry.
 */
static void __mm_free_page(x86_page_t* __s, int level) {

    DEBUG_ASSERT(__s);


    if ((*__s & X86_MMU_PG_U) == 0 || (*__s & X86_MMU_PG_G)) {

        return;

    } else {

        const uint64_t type = *__s & X86_MMU_PG_AP_TP_MASK;

        if (type == X86_MMU_PG_AP_TP_COW) {

            /* FIXME: a copy-on-write frame may still be referenced by another address space,
               and there is no per-frame reference count to tell, so it is deliberately leaked
               rather than risking a double free. Only reachable with CONFIG_DEMAND_PAGING. */
            return;

        } else if (type == X86_MMU_PG_AP_TP_SHARED) {

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

            if (!(s[i] & X86_MMU_PT_AP_PFB))
                continue;

            __mm_free_table(((uintptr_t)s[i]) & X86_MMU_ADDRESS_MASK, level - 1);

            __free_frame(((uintptr_t)s[i]) & X86_MMU_ADDRESS_MASK, X86_MMU_PAGESIZE);
        }

        s[i] = X86_MMU_CLEAR;
    }
}


#if defined(__x86_64__)
    #define MM_TOP_LEVEL 4
#elif defined(__i386__)
    #define MM_TOP_LEVEL 2
#else
    #error "Unsupported architecture!"
#endif


/**
 * @brief Builds an address space, either empty or cloned from a parent.
 *
 * @param parent The address space to clone, or the kernel space for an empty one.
 * @param flags The ARCH_VMM_CLONE_* flags the clone was asked for.
 * @return The new address space.
 */
__returns_nonnull vmm_address_space_t* arch_vmm_create_address_space(vmm_address_space_t* parent, int flags) {

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(parent->pm);

    vmm_address_space_t* dest = (vmm_address_space_t*)kcalloc(1, sizeof(vmm_address_space_t), GFP_KERNEL);

    if (unlikely(!dest)) {
        kpanicf("arch_vmm_create_address_space(): PANIC! Failed to allocate memory for address space!\n");
    }


    dest->pm = __alloc_frame(X86_MMU_PAGESIZE, true);



    size_t size = 0UL;

    scoped_lock(&parent->lock) {
        __mm_copy_table(parent->pm, dest->pm, &size, MM_TOP_LEVEL, flags);
    }


    dest->size = size;
    dest->flags = 0;

    atomic_store(&dest->refcount, 1);


    if (flags & ARCH_VMM_CLONE_USERSPACE) {

        dest->mmap.heap_start = parent->mmap.heap_start;
        dest->mmap.heap_end   = parent->mmap.heap_end;
        dest->mmap.heap_limit = parent->mmap.heap_limit;

        dest->brk.start = parent->brk.start;
        dest->brk.end   = parent->brk.end;

        memcpy(&dest->mmap.mappings, &parent->mmap.mappings, sizeof(mmap_mapping_t) * CONFIG_MMAP_MAX);

        shm_address_space_clone(parent, dest);

    } else {

        dest->mmap.heap_start = parent->mmap.heap_start;
        dest->mmap.heap_end   = parent->mmap.heap_start;
        dest->mmap.heap_limit = parent->mmap.heap_limit;

        dest->brk.start = 0UL;
        dest->brk.end   = 0UL;
    }


    spinlock_init_with_flags(&dest->lock, SPINLOCK_FLAGS_CPU_OWNER | SPINLOCK_FLAGS_RECURSIVE);


    if ((flags & ARCH_VMM_CLONE_DEMAND) && (flags & ARCH_VMM_CLONE_USERSPACE))
        arch_vmm_flush_all(parent);


    return dest;
}


/**
 * @brief Drops a reference to an address space, tearing it down when it was the last.
 *
 * @param space The address space to release.
 */
void arch_vmm_free_address_space(vmm_address_space_t* space) {

    DEBUG_ASSERT(space);
    DEBUG_ASSERT(space->pm);

    if (atomic_fetch_sub(&space->refcount, 1) > 1) {
        return;
    }


    if (space->flags & VMM_SPACE_STATIC) {

        atomic_store(&space->refcount, 0);
        return;
    }


    shm_address_space_release(space);


    vmm_address_space_t* kspace = &core->bsp.address_space;

    if (likely(space != kspace)) {

        if (current_task && current_task->address_space == space) {

            current_task->address_space = kspace;
            atomic_fetch_add(&kspace->refcount, 1);
        }

        if (space->pm == x86_get_cr3()) {
            x86_set_cr3(kspace->pm);
        }
    }


    scoped_lock(&space->lock) {
        __mm_free_table(space->pm, MM_TOP_LEVEL);
    }


    __free_frame(space->pm, X86_MMU_PAGESIZE);

    space->pm              = 0UL;
    space->size            = 0UL;
    space->mmap.heap_start = 0UL;
    space->mmap.heap_end   = 0UL;
    space->mmap.heap_limit = 0UL;

    space->brk.start = 0UL;
    space->brk.end   = 0UL;

    kfree(space);
}
