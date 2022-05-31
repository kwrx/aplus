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
#include <aplus/memory.h>
#include <aplus/ipc.h>
#include <aplus/hal.h>

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/vmm.h>
#include <arch/x86/intr.h>


void pagefault_handle(interrupt_frame_t* frame, uintptr_t cr2) {


#if defined(DEBUG) && DEBUG_LEVEL >= 2

    #define PFE(reason)     \
        { kprintf("x86-pfe: FAULT! address(0x%lX) cpu(%ld) pid(%d): %s\n", cr2, current_cpu->id, current_task ? current_task->tid : 0, reason); goto pfe; }

#else

    #define PFE(reason)     \
        { goto pfe; }

#endif



    uintptr_t pm = x86_get_cr3();
    
    if(unlikely(!pm))
        PFE("no memory mapping");


    uintptr_t pagesize = X86_MMU_PAGESIZE;
    uintptr_t s = cr2;



    {

        x86_page_t* d;


#if defined(__x86_64__)

        /* CR3-L4 */ 
        {
            d = &((x86_page_t*) arch_vmm_p2v(pm, ARCH_VMM_AREA_HEAP)) [(s >> 39) & 0x1FF];
        }

        /* PML4-L3 */
        {
            if(*d == X86_MMU_CLEAR)
                PFE("PML4-L3 doesn't not exist");

            d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 30) & 0x1FF];
        }


        /* HUGE_1GB */
        if(!(*d & X86_MMU_PG_PS)) {

            /* PDP-L2 */
            {
                if(*d == X86_MMU_CLEAR)
                    PFE("PDP-L2 doesn't not exist");

                d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 21) & 0x1FF];
            }

            /* HUGE_2MB */
            if(!(*d & X86_MMU_PG_PS)) {

                /* PD-L1 */
                {
                    if(*d == X86_MMU_CLEAR)
                        PFE("PD-L1 doesn't not exist");

                    d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 12) & 0x1FF];
                }

            } else
                pagesize = X86_MMU_HUGE_2MB_PAGESIZE;

        } else
            pagesize = X86_MMU_HUGE_1GB_PAGESIZE;


#elif defined(__i386__)

        /* CR3-L2 */
        {
            d = &((x86_page_t*) arch_vmm_p2v(space->pm, ARCH_VMM_AREA_HEAP)) [(s >> 22) & 0x3FF];
        }


        /* HUGE_4MB */
        if(!(*d & X86_MMU_PG_PS)) {

            /* PD-L1 */
            {
                if(*d == X86_MMU_CLEAR)
                    PFE("PD-L1 doesn't not exist");

                d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 12) & 0x3FF];
            }

        } else
            pagesize = X86_MMU_HUGE_2MB_PAGESIZE;

#endif

        /* Page Table */
        {

            if(*d == X86_MMU_CLEAR)
                PFE("page not present");

            // TODO: implement X86_MMU_PG_AP_TP_MMAP
            if(!(*d & X86_MMU_PG_AP_TP_COW))
                PFE("page fault cannot be handled, no copy on write flags found");



            //! Handle Copy on Write

            uintptr_t page = __alloc_page(pagesize, 0);

            if((*d & X86_MMU_ADDRESS_MASK) != 0) {

                memcpy (
                    (void*) arch_vmm_p2v(page, ARCH_VMM_AREA_HEAP),
                    (void*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP),
                    (size_t) pagesize
                );

                page |= X86_MMU_PG_RW;

            }
                
            *d = page | X86_MMU_PG_P 
                      | X86_MMU_PG_AP_PFB 
                      | X86_MMU_PG_AP_TP_PAGE
                      | ((*d & ~X86_MMU_ADDRESS_MASK) & ~(X86_MMU_PG_AP_TP_MASK));

        }

    }


    current_task->rusage.ru_majflt++;


#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("x86-pfe: handled page fault! cs(0x%lX), ip(0x%lX), sp(0x%lX), cr2(0x%lX) cr3(0x%lX) cpu(%ld) pid(%d)\n", frame->cs, frame->ip, frame->sp, cr2, x86_get_cr3(), current_cpu->id, current_task ? current_task->tid : 0);
#endif

    return;



pfe:

    kpanicf("x86-pfe: PANIC! cr2(0x%lX) cr3(0x%lX) gs(0x%llX) fs(0x%llX) cpu(%ld) pid(%d), cs(0x%lX), ip(0x%lX), sp(0x%lX), bp(0x%lX), ax(0x%lX), bx(0x%lX), cx(0x%lX), dx(0x%lX), si(0x%lX), di(0x%lX), errno(0x%lX) [%s %s %s %s %s %s %s %s]\n",
        cr2, 
        x86_get_cr3(), 
        x86_rdgsbase(),
        x86_rdfsbase(),
        current_cpu->id, 
        current_task ? current_task->tid : 0,
        frame->cs, 
        frame->ip, 
        frame->sp,
        frame->bp,
        frame->ax,
        frame->bx,
        frame->cx,
        frame->dx,
        frame->si,
        frame->di,
        frame->errno,
        frame->errno & X86_PF_P   ? "P"   : "NP",
        frame->errno & X86_PF_W   ? "W"   : "R",
        frame->errno & X86_PF_U   ? "U"   : "-",
        frame->errno & X86_PF_R   ? "R"   : "-",
        frame->errno & X86_PF_I   ? "I"   : "-",
        frame->errno & X86_PF_PK  ? "PK"  : "-",
        frame->errno & X86_PF_SS  ? "SS"  : "-",
        frame->errno & X86_PF_SGX ? "SGX" : "-"
    );


}