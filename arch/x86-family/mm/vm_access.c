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




/*!
 * @brief arch_vmm_access().
 *        Permission check for a virtual address.
 * 
 * @param space: address space.
 * @param virtaddr: virtual address.
 * @param mode: access mode.
 */
__nonnull(1)
int arch_vmm_access(vmm_address_space_t* space, uintptr_t virtaddr, int mode) {


    uintptr_t s = virtaddr;
    int e = 0;


    if(s & (X86_MMU_PAGESIZE - 1))
        s = (s & ~(X86_MMU_PAGESIZE - 1));



    #define check_or_fail(x)        \
        if(!(x)) {                  \
            e = -1;                 \
            goto out;               \
        }


    spinlock_lock(&space->lock);


    {

        x86_page_t* d;


#if defined(__x86_64__)

        /* CR3-L4 */ 
        {
            d = &((x86_page_t*) arch_vmm_p2v(space->pm, ARCH_VMM_AREA_HEAP)) [(s >> 39) & 0x1FF];
        }

        /* PML4-L3 */
        {
            check_or_fail(*d != X86_MMU_CLEAR);

            d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 30) & 0x1FF];
        }


        /* HUGE_1GB */
        if(!(*d & X86_MMU_PG_PS)) {

            /* PDP-L2 */
            {
                check_or_fail(*d != X86_MMU_CLEAR);

                d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 21) & 0x1FF];
            }

            /* HUGE_2MB */
            if(!(*d & X86_MMU_PG_PS)) {

                /* PD-L1 */
                {
                    check_or_fail(*d != X86_MMU_CLEAR);

                    d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 12) & 0x1FF];
                }

            }

        }

#elif defined(__i386__)

        /* CR3-L2 */
        {
            d = &((x86_page_t*) arch_vmm_p2v(space->pm, ARCH_VMM_AREA_HEAP)) [(s >> 22) & 0x3FF];
        }


        /* HUGE_4MB */
        if(!(*d & X86_MMU_PG_PS)) {

            /* PD-L1 */
            {
                check_or_fail(*d != X86_MMU_CLEAR);

                d = &((x86_page_t*) arch_vmm_p2v(*d & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP)) [(s >> 12) & 0x3FF];
            }

        }

#endif

        /* Page Table */
        {

            if(mode & R_OK) {
                if(!(*d & X86_MMU_PG_P)) {
                    if((*d & X86_MMU_PG_AP_TP_MASK) != X86_MMU_PG_AP_TP_COW) {
                        e = -1;
                    }
                }
            }

#if defined(__x86_64__)
            if(mode & X_OK) {
                if( (*d & X86_MMU_PT_NX) && !(*d & (1ULL << 47))) {
                    e = -1;
                }
            }
#endif

            if(mode & W_OK) {
                if(!(*d & X86_MMU_PG_RW)) {
                    if((*d & X86_MMU_PG_AP_TP_MASK) != X86_MMU_PG_AP_TP_COW) {
                        e = -1;
                    }
                }
            }

            if(mode & S_OK) {
                if( (*d & X86_MMU_PG_U)) {
                    e = -1;
                }
            }
                    
        }

    }


out:

    spinlock_unlock(&space->lock);

    return e;

}
