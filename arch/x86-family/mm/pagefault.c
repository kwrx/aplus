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
 * @brief x86_vmm_resolve().
 *        Materialise a copy-on-write or demand-paged entry.
 *
 * Shared by the page fault path and by arch_vmm_lock(), which pre-faults a user range before
 * the kernel dereferences it. The caller must hold the address space lock and is responsible
 * for invalidating the translation afterwards.
 *
 * @param pm: physical address of the root table to walk.
 * @param virtaddr: faulting address.
 * @param err: the access being attempted, as X86_PF_* bits. arch_vmm_lock() synthesises
 *             these; the fault path passes the hardware error code verbatim.
 * @param reason: optional. Set to a human-readable cause when the entry cannot be resolved.
 *
 * @return 0 if the entry now maps a private frame, -1 if the access must not be satisfied.
 */
int x86_vmm_resolve(uintptr_t pm, uintptr_t virtaddr, uint64_t err, const char** reason) {

#define FAIL(r)              \
    {                        \
        if (reason)          \
            *reason = (r);   \
        return -1;           \
    }

    if (unlikely(!pm))
        FAIL("no memory mapping");


    uintptr_t pagesize = X86_MMU_WALK_ANY;

    x86_page_t* d = x86_vmm_walk(pm, virtaddr, &pagesize, 0, 0, NULL);

    if (unlikely(!d))
        FAIL("no page table for address");

    if (*d == X86_MMU_CLEAR)
        FAIL("page not present");

    // TODO: implement X86_MMU_PG_AP_TP_MMAP
    if ((*d & X86_MMU_PG_AP_TP_MASK) != X86_MMU_PG_AP_TP_COW)
        FAIL("page fault cannot be handled, no copy on write flags found");


    /* Decide whether resolving the entry would actually satisfy the access. Without this the
       handler resolves the entry, returns, and the very same access faults again on identical
       terms -- an unkillable fault loop rather than the SIGSEGV the process has earned. */

    if (err & X86_PF_R)
        FAIL("reserved bit set in a page table entry");

    if ((err & X86_PF_U) && !(*d & X86_MMU_PG_U))
        FAIL("user access to a supervisor page");

    if ((err & X86_PF_I) && (*d & X86_MMU_PT_NX))
        FAIL("instruction fetch from a no-execute page");

    if ((err & X86_PF_W) && !(*d & X86_MMU_PG_AP_COW_RW))
        FAIL("write to a read-only page");


    const uintptr_t src = *d & X86_MMU_ADDRESS_MASK;

    /* A demand page has nothing to copy from, so the frame must be zeroed. It used to be
       handed to userspace as allocated, still holding whatever the previous owner wrote. */
    uintptr_t page = __try_alloc_frame(pagesize, src == 0);

    if (unlikely(page == X86_MMU_FRAME_NONE))
        FAIL("out of physical memory");


    if (src != 0) {

        memcpy((void*)arch_vmm_p2v(page, ARCH_VMM_AREA_HEAP), (void*)arch_vmm_p2v(src, ARCH_VMM_AREA_HEAP), (size_t)pagesize);
    }


    uint64_t b = (*d & ~X86_MMU_ADDRESS_MASK) & ~X86_MMU_PG_AP_TP_MASK;

    b |= X86_MMU_PG_P | X86_MMU_PG_AP_PFB | X86_MMU_PG_AP_TP_PAGE;

    /* Restore the permission the mapping was created with rather than granting write access
       unconditionally: a read-only mapping that survived a fork must stay read-only. */
    if (*d & X86_MMU_PG_AP_COW_RW)
        b |= X86_MMU_PG_RW;
    else
        b &= ~X86_MMU_PG_RW;


    *d = page | b;

    return 0;

#undef FAIL
}


__nonnull(1) int pagefault_handle(interrupt_frame_t* frame, uintptr_t cr2) {

    /* The fault happened against whatever CR3 holds, which is this task's address space.
       Take its lock so two threads sharing it cannot both resolve the same entry and leak
       one of the two frames they allocate. */
    vmm_address_space_t* space = current_task ? current_task->address_space : NULL;

    const char* reason = "unknown";

    int e = 0;


    if (likely(space))
        spinlock_lock(&space->lock);

    e = x86_vmm_resolve(x86_get_cr3(), cr2, frame->errno, &reason);

    if (likely(e == 0)) {

        /* The stale read-only translation is still cached; without this the write that
           faulted re-faults, and the entry is no longer COW, so it takes the unhandled path. */
        if (likely(space))
            arch_vmm_flush(space, cr2);
        else
            __asm__ __volatile__("invlpg (%0)" ::"r"(cr2) : "memory");
    }

    if (likely(space))
        spinlock_unlock(&space->lock);


    if (likely(e == 0)) {

        /* A copy-on-write or demand fault is serviced without I/O, so it is a minor fault. */
        if (likely(current_task))
            current_task->rusage.ru_minflt++;

#if DEBUG_LEVEL_TRACE
        kprintf("x86-pfe: handled page fault at 0x%lX! cs(0x%lX), ip(0x%lX), sp(0x%lX), cr3(0x%lX) cpu(%ld) pid(%d)\n", cr2, frame->cs, frame->ip, frame->sp, x86_get_cr3(), current_cpu->id, current_task ? current_task->tid : 0);
#endif

        return 0;
    }


#if DEBUG_LEVEL_TRACE
    kprintf("x86-pfe: FAULT! address(0x%lX) cpu(%ld) pid(%d): %s\n", cr2, current_cpu->id, current_task ? current_task->tid : 0, reason);
#endif

    if (x86_intr_is_user_mode(frame))
        return -1;

    kpanicf(
        "x86-pfe: PANIC! %s cr2(0x%lX) cr3(0x%lX) gs(0x%llX) fs(0x%llX) cpu(%ld) pid(%d), cs(0x%lX), ip(0x%lX), sp(0x%lX), bp(0x%lX), ax(0x%lX), bx(0x%lX), cx(0x%lX), dx(0x%lX), si(0x%lX), di(0x%lX), errno(0x%lX) [%s %s %s %s %s %s %s %s]\n",
        reason, cr2, x86_get_cr3(), x86_rdmsr(X86_MSR_GSBASE), x86_rdmsr(X86_MSR_FSBASE), current_cpu->id, current_task ? current_task->tid : 0, frame->cs, frame->ip, frame->sp, frame->bp, frame->ax, frame->bx, frame->cx, frame->dx, frame->si, frame->di,
        frame->errno,
        frame->errno & X86_PF_P ? "P" : "NP",   // Page present/not present
        frame->errno & X86_PF_W ? "W" : "R",    // Write/Read
        frame->errno & X86_PF_U ? "U" : "-",    // User/Supervisor
        frame->errno & X86_PF_R ? "R" : "-",    // Reserved bit
        frame->errno & X86_PF_I ? "I" : "-",    // Instruction fetch
        frame->errno & X86_PF_PK ? "PK" : "-",  // Protection key
        frame->errno & X86_PF_SS ? "SS" : "-",  // Shadow stack
        frame->errno & X86_PF_SGX ? "SGX" : "-" // SGX
    );
}
