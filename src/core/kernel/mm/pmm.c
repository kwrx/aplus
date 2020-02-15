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
                                                                        
#include <aplus/core/base.h>
#include <aplus/core/debug.h>
#include <aplus/core/ipc.h>

#define PML2_PAGESIZE               (128 * 1024 * 1024)     //? 128MiB
#define PML1_PAGESIZE               (4096)                  //? 4KiB

#define PML2_MAX_ENTRIES            (4096)
#define PML1_MAX_ENTRIES            (4096)


/*!
 * @brief pml2_bitmap[].
 *        Physical Page Map Level 2.
 * 
 * // TODO: Insert description
 */
static uintptr_t pml2_bitmap[4096];

/*!
 * @brief pml2_pusage[].
 *        Number of allocated pages in Page Map Level 1.
 */
static uint16_t pml2_pusage[4096];

static spinlock_t pml2_lock;



/*!
 * @brief pmm_claim_area().
 *        Mark user defined area as reserved.
 * 
 * @param physaddr: Physical Address of Memory Area.
 * @param size:     Size of Memory Area in bytes.
 */
void pmm_claim_area(uintptr_t physaddr, size_t size) {

    if(physaddr + size > pmm_max_size)
        kpanicf("pmm: PANIC! Memory Area (%p-%p) is greater than max memory available (%p)\n", physaddr, physaddr + size, pmm_max_size);


    for(uint64_t p = physaddr; p < (physaddr + size); p += PML1_PAGESIZE) {

        int pml2_index = p >> 27;

        BUG_ON(pml2_index > PML2_MAX_ENTRIES);
        BUG_ON(pml2_bitmap[pml2_index / sizeof(uint64_t)] & (pml2_index % sizeof(uint64_t)) == 0)




        uint64_t* pml1_bitmap = (uint64_t*) arch_v2p(pml2_index * PML2_PAGESIZE);


    }

}




