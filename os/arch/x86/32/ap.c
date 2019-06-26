/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <aplus/ipc.h>
#include <aplus/smp.h>
#include <arch/x86/mm.h>
#include <arch/x86/smp.h>
#include <arch/x86/cpu.h>
#include <arch/x86/apic.h>
#include <arch/x86/intr.h>
#include <stdint.h>
#include <string.h>

#include "mp/ap.h"


spinlock_t ap_interlock;
volatile uint32_t ap_cores;



void ap_main(void) {
    __lock(&ap_interlock, {
    
        x86_lgdt();
        x86_lidt();
        x86_sti();
        
        apic_enable();

        __atomic_add_fetch(&ap_cores, 1, __ATOMIC_ACQ_REL);
    });

    smp_setup(0);
}

void ap_init(void) {
    DEBUG_ASSERT(sizeof(ap_bootstrap) < PAGE_SIZE);

    spinlock_init(&ap_interlock);
    ap_cores = 0;

    ap_bootstrap_header_t* header = (ap_bootstrap_header_t*) (&ap_bootstrap[AP_BOOTSTRAP_HEADER & 0xFFF]);
    DEBUG_ASSERT(header->ap_magic == AP_BOOTSTRAP_MAGIC);
    DEBUG_ASSERT(header->ap_start == AP_BOOTSTRAP_CODE);


    pmm_claim (
        AP_BOOTSTRAP_CODE, 
        AP_BOOTSTRAP_CODE + PAGE_SIZE - 1
    );

    x86_map_page (
        (x86_page_t*) (CONFIG_KERNEL_BASE + x86_get_cr3()),
        AP_BOOTSTRAP_CODE, 
        AP_BOOTSTRAP_CODE >> 12,
        X86_MMU_PG_P | X86_MMU_PG_RW
    );

    memcpy (
        (void*) AP_BOOTSTRAP_CODE,
        (void*) ap_bootstrap,
        header->ap_end - header->ap_start + 1
    );

    memcpy (
        (void*) AP_BOOTSTRAP_HEADER,
        (void*) header,
        sizeof(header)
    );


    header = (ap_bootstrap_header_t*) (AP_BOOTSTRAP_HEADER);
    header->ap_main = (uintptr_t) &ap_main;
    header->ap_cr3 = x86_get_cr3();
    header->ap_stack = 0;
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

void ap_stack(uintptr_t p) {
    ap_bootstrap_header_t* header = (ap_bootstrap_header_t*) (AP_BOOTSTRAP_HEADER);
    DEBUG_ASSERT(header->ap_magic == AP_BOOTSTRAP_MAGIC);

    header->ap_stack = p;
}
