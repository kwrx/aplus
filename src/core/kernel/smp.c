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
#include <aplus/core/memory.h>
#include <aplus/core/hal.h>
#include <aplus/core/smp.h>


cpu_t* smp_get_current_cpu(void) {

    uint64_t id;
    
    if((id = arch_cpu_get_current_id()) == SMP_CPU_BOOTSTRAP_ID)
        return &core->bsp;
    

    int i;
    for(i = 0; i < SMP_CPU_MAX; i++) {

        if(id == core->cpu.cores[i].id)
            return &core->cpu.cores[i];

    }


    kpanicf("smp_get_current_cpu(): PANIC! cpu id(%d) not found!\n", id);

}


cpu_t* smp_get_cpu(int index) {
    
    DEBUG_ASSERT(index >= 0);
    DEBUG_ASSERT(index <= SMP_CPU_MAX - 1);

    return &core->cpu.cores[index];
}


void smp_init() {

    int i;
    for(i = 1; i < SMP_CPU_MAX; i++) {

        if(!(core->cpu.cores[i].flags & SMP_CPU_FLAGS_AVAILABLE))
            continue;

        if( (core->cpu.cores[i].flags & SMP_CPU_FLAGS_ENABLED))
            continue;


        arch_cpu_startup(i);

    }

}
