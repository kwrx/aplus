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

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>
#include <arch/x86/vmm.h>



__nonnull(1) void arch_vmm_flush(vmm_address_space_t* space, uintptr_t virtaddr) {

    if (space->pm == x86_get_cr3() || virtaddr >= X86_MMU_USERSPACE_END) {
        __asm__ __volatile__("invlpg (%0)" ::"r"(virtaddr) : "memory");
    }
}


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


__nonnull(1) void arch_vmm_flush_all(vmm_address_space_t* space) {

    if (space->pm == x86_get_cr3())
        x86_vmm_tlb_load(space);
}


__nonnull(1) void x86_vmm_tlb_load(vmm_address_space_t* space) {

    const long irq = arch_intr_disable();

    cpu_t* cpu = current_cpu;

    spinlock_lock(&cpu->tlb.lock);

    const uint64_t gen = atomic_load(&space->tlb.gen);

    x86_set_cr3(space->pm);

    cpu->tlb.space = space;
    cpu->tlb.gen   = gen;

    spinlock_unlock(&cpu->tlb.lock);

    arch_intr_restore(irq);
}


__nonnull(1) void x86_vmm_tlb_sync(vmm_address_space_t* space) {

    cpu_t* cpu = current_cpu;

    if (likely(cpu->tlb.space == space && cpu->tlb.gen == atomic_load(&space->tlb.gen)))
        return;

    x86_vmm_tlb_load(space);
}


__nonnull(1) uint64_t x86_vmm_tlb_bump(vmm_address_space_t* space) {
    return atomic_fetch_add(&space->tlb.gen, 1) + 1;
}


static bool __tlb_passed(vmm_address_space_t* space, uint64_t gen, bool skip_self) {

    cpu_foreach(cpu) {

        if (skip_self && cpu == current_cpu)
            continue;

        spinlock_lock(&cpu->tlb.lock);

        const bool stale = cpu->tlb.space == space && cpu->tlb.gen < gen;

        spinlock_unlock(&cpu->tlb.lock);

        if (stale)
            return false;
    }

    return true;
}


static inline uintptr_t __tlb_frame_entry(uintptr_t frame, uintptr_t pagesize) {

#if defined(__x86_64__)
    if (pagesize == X86_MMU_HUGE_1GB_PAGESIZE)
        return frame | 2;
#endif

    if (pagesize == X86_MMU_HUGE_2MB_PAGESIZE)
        return frame | 1;

    return frame;
}


static void __tlb_free_batches(vmm_tlb_batch_t* batch) {

    while (batch) {

        vmm_tlb_batch_t* next = batch->next;

        for (size_t i = 0; i < batch->count; i++) {

            const uintptr_t e = batch->entries[i];

            if (batch->kind == VMM_TLB_BATCH_SHM) {
                shm_release((int)e);
                continue;
            }

            switch (e & 3) {
#if defined(__x86_64__)
                case 2:
                    __free_frame(e & X86_MMU_ADDRESS_MASK, X86_MMU_HUGE_1GB_PAGESIZE);
                    break;
#endif
                case 1:
                    __free_frame(e & X86_MMU_ADDRESS_MASK, X86_MMU_HUGE_2MB_PAGESIZE);
                    break;
                default:
                    __free_frame(e & X86_MMU_ADDRESS_MASK, X86_MMU_PAGESIZE);
                    break;
            }
        }

        kfree(batch);

        batch = next;
    }
}


static vmm_tlb_batch_t* __tlb_new_batch(vmm_tlb_batch_t** batches, uint32_t kind) {

    vmm_tlb_batch_t* b = (vmm_tlb_batch_t*)kmalloc(sizeof(vmm_tlb_batch_t), GFP_KERNEL);

    if (unlikely(!b))
        return NULL;

    b->next  = *batches;
    b->gen   = 0;
    b->count = 0;
    b->kind  = kind;

    return (*batches = b);
}


void x86_vmm_tlb_collect(vmm_tlb_batch_t** batches, uintptr_t frame, uintptr_t pagesize) {

    vmm_tlb_batch_t* b = *batches;

    if (!b || b->count == VMM_TLB_BATCH_MAX)
        b = __tlb_new_batch(batches, VMM_TLB_BATCH_FRAMES);

    if (unlikely(!b))
        return;

    b->entries[b->count++] = __tlb_frame_entry(frame, pagesize);
}


static void __tlb_defer(vmm_address_space_t* space, vmm_tlb_batch_t* batches, uint64_t gen) {

    vmm_tlb_batch_t* tail = batches;

    for (;;) {

        tail->gen = gen;

        if (!tail->next)
            break;

        tail = tail->next;
    }

    scoped_lock(&space->tlb.lock) {
        tail->next          = space->tlb.deferred;
        space->tlb.deferred = batches;
    }
}


void x86_vmm_tlb_retire(vmm_address_space_t* space, vmm_tlb_batch_t* batches, uint64_t gen) {

    if (batches) {

        if (__tlb_passed(space, gen, true))
            __tlb_free_batches(batches);
        else
            __tlb_defer(space, batches, gen);
    }

    arch_vmm_reclaim(space);
}


void arch_vmm_release_shm(vmm_address_space_t* space, int id) {

    const uint64_t gen = atomic_load(&space->tlb.gen);

    if (__tlb_passed(space, gen, true)) {
        shm_release(id);
        return;
    }


    vmm_tlb_batch_t* batches = NULL;

    if (unlikely(!__tlb_new_batch(&batches, VMM_TLB_BATCH_SHM)))
        return;

    batches->entries[batches->count++] = (uintptr_t)id;

    __tlb_defer(space, batches, gen);
}


void arch_vmm_reclaim(vmm_address_space_t* space) {

    if (likely(!__atomic_load_n(&space->tlb.deferred, __ATOMIC_RELAXED)))
        return;


    vmm_tlb_batch_t* ready = NULL;

    scoped_lock(&space->tlb.lock) {

        vmm_tlb_batch_t** p = &space->tlb.deferred;

        while (*p) {

            vmm_tlb_batch_t* b = *p;

            if (__tlb_passed(space, b->gen, false)) {

                *p      = b->next;
                b->next = ready;
                ready   = b;

            } else {

                p = &b->next;
            }
        }
    }

    __tlb_free_batches(ready);
}


void x86_vmm_tlb_drain(vmm_address_space_t* space) {

    vmm_tlb_batch_t* all = NULL;

    scoped_lock(&space->tlb.lock) {
        all                 = space->tlb.deferred;
        space->tlb.deferred = NULL;
    }

    __tlb_free_batches(all);
}
