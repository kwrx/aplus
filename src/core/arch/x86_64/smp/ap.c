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



spinlock_t ap_interlock;
volatile uint32_t ap_cores;


void ap_main(void) {

    __atomic_add_fetch(&ap_cores, 1, __ATOMIC_ACQ_REL);
    __asm__ __volatile__("cli; monitor; mwait");
}


ap_header_t* ap_get_header(void) {

    ap_header_t* ap = (ap_header_t*) arch_vmm_p2v(AP_BOOT_OFFSET + AP_BOOT_HEADER_OFFSET, ARCH_VMM_AREA_HEAP);

    DEBUG_ASSERT(ap);
    DEBUG_ASSERT(ap->magic == AP_BOOT_HEADER_MAGIC);

    return ap;
}


void ap_init() {

    spinlock_init(&ap_interlock);
    ap_cores = 0;




#if defined(DEBUG) && DEBUG_LEVEL >= 4
    #define cpy(offset, s, e)                                                       \
        kprintf("x86-ap: copy %s to %p (%d bytes)\n", #s, offset,                   \
                        (size_t) ((uintptr_t) (e) - (uintptr_t) (s)));              \
        memcpy (                                                                    \
            (void*) arch_vmm_p2v(offset, ARCH_VMM_AREA_HEAP),                       \
            (void*) (s),                                                            \
            (size_t) ((uintptr_t) (e) - (uintptr_t) (s))                            \
        )
#else
    #define cpy(offset, s, e)                                                       \
        memcpy (                                                                    \
            (void*) arch_vmm_p2v(offset, ARCH_VMM_AREA_HEAP),                       \
            (void*) (s),                                                            \
            (size_t) ((uintptr_t) (e) - (uintptr_t) (s))                            \
        )
#endif




    extern int __ap16_start;
    extern int __ap32_start;
    extern int __ap64_start;
    extern int __ap16_end;
    extern int __ap32_end;
    extern int __ap64_end;

    //* Copy AP Startup Code
    cpy(AP_BOOT_OFFSET + AP_BOOT_AP16_OFFSET, &__ap16_start, &__ap16_end);
    cpy(AP_BOOT_OFFSET + AP_BOOT_AP32_OFFSET, &__ap32_start, &__ap32_end);
    cpy(AP_BOOT_OFFSET + AP_BOOT_AP64_OFFSET, &__ap64_start, &__ap64_end);


    extern int __ap_gdt32_start;
    extern int __ap_gdt32_end;

    //* Copy AP Startup Data
    cpy(AP_BOOT_OFFSET + AP_BOOT_GDT32_OFFSET, &__ap_gdt32_start, &__ap_gdt32_end);


    
    extern int __ap_header_start;
    extern int __ap_header_end;
    
    //* Copy AP Header Data
    cpy(AP_BOOT_OFFSET + AP_BOOT_HEADER_OFFSET, &__ap_header_start, &__ap_header_end);


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
