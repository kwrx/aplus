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
#include <aplus/core/base.h>
#include <aplus/core/multiboot.h>
#include <aplus/core/debug.h>
#include <aplus/core/memory.h>
#include <aplus/core/ipc.h>
#include <aplus/core/hal.h>

#include <arch/x86/cpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/intr.h>
#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>
#include <arch/x86/smp.h>



spinlock_t ap_interlock = 0;
volatile uint32_t ap_cores = 0;


__percpu
void ap_bmain(uint64_t magic, uint64_t cpu) {

    DEBUG_ASSERT(magic == AP_BOOT_HEADER_MAGIC);
    DEBUG_ASSERT(cpu != SMP_CPU_BOOTSTRAP_ID);


    __lock(&ap_interlock, {

        //* Initialize AP CPU
        arch_cpu_init(cpu);

        //* Enable Local APIC
        apic_enable();

        //* Enable Interrupts
        __asm__ __volatile__("sti");

    });

    __atomic_add_fetch(&ap_cores, 1, __ATOMIC_ACQ_REL);

}


ap_header_t* ap_get_header(void) {

    extern int __ap_begin;
    extern int __ap_end;


    ap_header_t* ap = (ap_header_t*) arch_vmm_p2v(AP_BOOT_OFFSET + (size_t) ((uintptr_t) (&__ap_end) - (uintptr_t) (&__ap_begin)) - sizeof(ap_header_t), ARCH_VMM_AREA_HEAP);

    DEBUG_ASSERT(ap);
    DEBUG_ASSERT(ap->magic == AP_BOOT_HEADER_MAGIC);

    return ap;
}


void ap_init() {


    extern int __ap_begin;
    extern int __ap_end;

    memcpy (
        (void*) arch_vmm_p2v(AP_BOOT_OFFSET, ARCH_VMM_AREA_HEAP),
        (void*) (&__ap_begin),
        (size_t) ((uintptr_t) (&__ap_end) - (uintptr_t) (&__ap_begin))
    );


#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("ap: copied AP Startup code at %p (%d bytes)\n", AP_BOOT_OFFSET, (size_t) ((uintptr_t) (&__ap_end) - (uintptr_t) (&__ap_begin)));
#endif

}


int ap_check(int core, int timeout) {

    int us;
    for(us = 0; us < timeout; us += 200) {

        __atomic_thread_fence(__ATOMIC_SEQ_CST);

        if(!(ap_cores < core))
            return 1;

        arch_timer_delay(200);

    }

    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    return (!(ap_cores < core));
}
