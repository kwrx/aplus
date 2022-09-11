/*                                                                      
 * Author(s):                                                           
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
 *                                                                      
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
                                                                        
#ifndef _APLUS_SMP_H
#define _APLUS_SMP_H

#ifndef __ASSEMBLY__
#include <time.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/task.h>

#define SMP_CPU_MAX                     256
#define SMP_CPU_MAX_FEATURES            32
#define SMP_CPU_BOOTSTRAP_ID            0

#define SMP_CPU_FLAGS_AVAILABLE         (1 << 0)
#define SMP_CPU_FLAGS_ENABLED           (1 << 1)
#define SMP_CPU_FLAGS_WAIT              (1 << 2)
#define SMP_CPU_FLAGS_SLEEP             (1 << 3)


#define current_cpu     \
    (smp_get_current_cpu())

#define current_task    \
    (smp_get_current_cpu())->sched_running



#define cpu_has(index, feature)     \
    (core->cpu.cores[index].features[feature >> 5U] & (1U << (feature & 0x1FU)))

#define boot_cpu_has(feature)       \
    cpu_has(SMP_CPU_BOOTSTRAP_ID, feature)

#define current_cpu_has(feature)    \
    (current_cpu->features[feature >> 5U] & (1U << (feature & 0x1FU)))


#define cpu_foreach(e)                                                              \
        for(cpu_t* e = &core->cpu.cores[0]; e; e = NULL)                            \
            for(int __i = 0; __i < SMP_CPU_MAX; __i++)                              \
                if(!((e = &core->cpu.cores[__i])->flags & SMP_CPU_FLAGS_ENABLED))   \
                    continue;                                                       \
                else

#define cpu_foreach_if(e, cond)                                                     \
        for(cpu_t* e = &core->cpu.cores[0]; e; e = NULL)                            \
            for(int __i = 0; __i < SMP_CPU_MAX && (cond); __i++)                    \
                if(!((e = &core->cpu.cores[__i])->flags & SMP_CPU_FLAGS_ENABLED))   \
                    continue;                                                       \
                else




typedef uint8_t irq_t;
typedef uint64_t cpuid_t;


typedef struct cpu {

    cpuid_t id;

    void* frame;
    void* tss;
    void* kstack;
    void* ustack;

    int errno;

    uint64_t node;
    uint64_t archid;
    uint64_t flags;
    uint64_t features[SMP_CPU_MAX_FEATURES];
    
    vmm_address_space_t address_space;
    
    task_t*  sched_running;
    task_t*  sched_queue;
    size_t   sched_count;

    uint64_t ticks;
    struct timespec uptime;

    spinlock_t global_lock;
    spinlock_t sched_lock;

} cpu_t;



__BEGIN_DECLS


cpu_t* smp_get_current_cpu(void) __returns_nonnull;
cpu_t* smp_get_cpu(int) __returns_nonnull;

void smp_init();

__END_DECLS

#endif
#endif
