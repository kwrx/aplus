/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
 *                                                                      
 * This file is part of aPlus.                                          
 *                                                                      
 * aPlus is free software: you can redistribute it and/or modify        
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or    
 * (at your option) any later version.                                  
 *                                                                      
 * aPlus is distributed in the hope that it will be useful,             
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        
 * GNU General Public License for more details.                         
 *                                                                      
 * You should have received a copy of the GNU General Public License    
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.       
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




void arch_vmm_clone(vmm_address_space_t* dest, vmm_address_space_t* src, int flags) {

    DEBUG_ASSERT(src);
    DEBUG_ASSERT(src->pm);
    DEBUG_ASSERT(dest);

    uint64_t size = 0ULL;



    if(dest->pm == 0UL)
        dest->pm = __alloc_page(X86_MMU_PAGESIZE, 1);


    x86_page_t __fork_data(x86_page_t* __s, int on_demand, int level) {

        DEBUG_ASSERT(__s);


        uintptr_t pagesize;

        switch(level) {

            case 1: pagesize = X86_MMU_PAGESIZE; break;
            case 2: pagesize = X86_MMU_HUGE_2MB_PAGESIZE; break;
#if defined(__x86_64__)
            case 3: pagesize = X86_MMU_HUGE_1GB_PAGESIZE; break;
#endif
            default: kpanicf("arch_vmm_clone(): PANIC! Page map level too high or invalid: %d\n", level);

        }

        size += pagesize >> 12;

// FIXME: broken DEMAND_PAGING implementation
// #if defined(CONFIG_DEMAND_PAGING)
//         if(on_demand) {
            
//             return (*__s = (*__s & ~(X86_MMU_PG_RW | X86_MMU_PG_AP_TP_MASK)) | X86_MMU_PG_AP_TP_COW);
            
//         } else
// #endif
        {
            

            uintptr_t page = __alloc_page(pagesize, 0);

            memcpy (
                (void*) arch_vmm_p2v(page, ARCH_VMM_AREA_HEAP),
                (void*) arch_vmm_p2v(*__s & X86_MMU_ADDRESS_MASK, ARCH_VMM_AREA_HEAP),
                (size_t) pagesize
            );

            
            return page | (*__s & ~X86_MMU_ADDRESS_MASK);

        }
    }
    

    void __clone_pg(x86_page_t* __s, x86_page_t* __d, int level) {

        DEBUG_ASSERT(__s);
        DEBUG_ASSERT(__d);

        #define has(flag)   \
            (flags & flag)


        if((*__s & X86_MMU_PG_U) == 0)
           
            *__d = *__s;

        else {

            if(has(ARCH_VMM_CLONE_VM)) {

                *__d = *__s;

            } else {

                if((*__s & X86_MMU_PG_AP_TP_MASK) == X86_MMU_PG_AP_TP_COW) {
                    
                    *__d = *__s;

                } else {
                
                    *__d = __fork_data(__s, has(ARCH_VMM_CLONE_DEMAND), level);
                
                }

            }

        }

    }


    void __clone_pt(uintptr_t __s, uintptr_t __d, int level) {

        DEBUG_ASSERT(__s);
        DEBUG_ASSERT(__d);


        x86_page_t* s = (x86_page_t*) arch_vmm_p2v(__s, ARCH_VMM_AREA_HEAP);
        x86_page_t* d = (x86_page_t*) arch_vmm_p2v(__d, ARCH_VMM_AREA_HEAP);

        int i;
        for(i = 0; i < X86_MMU_PT_ENTRIES; i++) {

            //? Skip unallocated pages
            if(s[i] == X86_MMU_CLEAR)
                continue;


            if((s[i] & X86_MMU_PG_PS) || (level == 1))
                __clone_pg(&s[i], &d[i], level);

            else {

                d[i] = __alloc_page(X86_MMU_PAGESIZE, 1) | (s[i] & ~X86_MMU_ADDRESS_MASK) | X86_MMU_PG_AP_PFB;

                __clone_pt(((uintptr_t) s[i]) & X86_MMU_ADDRESS_MASK, ((uintptr_t) d[i]) & X86_MMU_ADDRESS_MASK, level - 1);

            }

        }

    }

#if defined(__x86_64__)

    __lock(&src->lock,
        __clone_pt(src->pm, dest->pm, 4));

#elif defined(__i386__)

    __lock(&src->lock,
        __clone_pt(src->pm, dest->pm, 2));

#endif



    dest->size = size;
    dest->refcount = 1;

    dest->mmap.heap_start = src->mmap.heap_start;
    dest->mmap.heap_end   = src->mmap.heap_end;
    
    memcpy(&dest->mmap.mappings, &src->mmap.mappings, sizeof(mmap_mapping_t) * CONFIG_MMAP_MAX);


    spinlock_init(&dest->lock);

}

