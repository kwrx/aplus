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
                                                                        
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/hal.h>

__returns_nonnull
cpu_t* smp_get_current_cpu(void) {

    uint64_t id;
    
    if((id = arch_cpu_get_current_id()) == SMP_CPU_BOOTSTRAP_ID)
        return &core->bsp;
    

    
    DEBUG_ASSERT(id >= 0);
    DEBUG_ASSERT(id <= SMP_CPU_MAX - 1);

    if(core->cpu.cores[id].flags & SMP_CPU_FLAGS_ENABLED) {
        return &core->cpu.cores[id];
    }


    kpanicf("smp_get_current_cpu(): PANIC! wrong cpu id(%ld)\n", id);

}

__returns_nonnull
cpu_t* smp_get_cpu(int index) {
    
    DEBUG_ASSERT(index >= 0);
    DEBUG_ASSERT(index <= SMP_CPU_MAX - 1);

    return &core->cpu.cores[index];
}


void smp_init() {


    size_t max_cpus = SMP_CPU_MAX;

    if(strstr(core->boot.cmdline, "smp=off"))
        return;

    if(strstr(core->boot.cmdline, "smp=")) {

        max_cpus = atoll(strstr(core->boot.cmdline, "smp=") + 4);

        if(max_cpus > SMP_CPU_MAX || max_cpus < 1) {
            max_cpus = SMP_CPU_MAX;
        }

    }


    for(size_t i = 1; i < SMP_CPU_MAX; i++) {

        if(!(core->cpu.cores[i].flags & SMP_CPU_FLAGS_AVAILABLE))
            continue;

        if( (core->cpu.cores[i].flags & SMP_CPU_FLAGS_ENABLED))
            continue;

        if(--max_cpus == 0)
            break;


        arch_cpu_startup(i);

        kprintf("smp: cpu #%zd is online\n", i);

    }

}
