/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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
#include <aplus/timer.h>
#include <aplus/smp.h>
#include <arch/x86/mm.h>
#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>
#include <arch/x86/cpu.h>
#include <arch/x86/smp.h>
#include <string.h>


extern uintptr_t early_stack;
static cpu_t cpus[CPU_MAX];


cpu_t* get_current_cpu(void) {
    return get_cpu(apic_get_id());
}

cpu_t* get_cpu(int id) {   
    DEBUG_ASSERT(id < CPU_MAX);
    DEBUG_WARNING(cpus[id].flags & CPU_FLAGS_ENABLED);
    
    return &cpus[id];
}


void smp_main(int bsp) {
    DEBUG_ASSERT(!(cpus[apic_get_id()].flags & CPU_FLAGS_ENABLED));

    static char* __argv[] = { "[core]", NULL };
    static char* __envp[] = { NULL };

    int i = apic_get_id();
    cpus[i].id = i;
    cpus[i].flags = CPU_FLAGS_ENABLED | (bsp ? CPU_FLAGS_BSP : 0);
    cpus[i].ticks = 0;
    cpus[i].task.current = &cpus[i].task.core;


    #define _ (cpus[i].task.core)

    memset(&_, 0, sizeof(_));
    _.argv = (char**) __argv;
    _.environ = (char**) __envp;

    _.pid =
    _.sid =
    _.pgid =
    _.tgid = sched_nextpid();

    _.uid =
    _.gid =
    _.euid =
    _.egid = 0;

    _.status = TASK_STATUS_READY;
    _.priority = TASK_PRIO_REG;
    _.affinity = ~(1 << apic_get_id());

    _.context.ip = 0;
    _.context.sp = 0;

    _.root =
    _.cwd =
    _.exe = NULL;

    _.umask = 000;


    _.aspace = PTR_REF(&_.__aspace);
    _.aspace->start = CONFIG_KERNEL_BASE;
    _.aspace->end = mbd->memory.start;
    _.aspace->used = 0;
    _.aspace->vmmpd = x86_get_cr3();

    spinlock_init(&_.aspace->lock);


    _.parent = NULL;
    _.next = NULL;

    spinlock_init(&_.lock);


    if(bsp)
        return;

    cmain();
}

void smp_init(void) {
    
    ap_init();


    int i;
    for(i = 0; i < mbd->cpu.max_cores; i++) {
        if(mbd->cpu.cores[i].id == apic_get_id())
            continue;

        ap_stack(((uintptr_t) &early_stack) - (i * PAGE_SIZE));

        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_HI, mbd->cpu.cores[i].id << 24);
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_LO, (5 << 8) | (1 << 14));

        while (
            ((mmio_r32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_HI) >> 12) & 1)
        ) ;

        arch_timer_delay(10000);
        
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_HI, mbd->cpu.cores[i].id << 24);
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_LO, ((AP_BOOTSTRAP_CODE >> 12) & 0xFF) | (6 << 8) | (1 << 14));
        
        arch_timer_delay(200);

        if(ap_check(i, 500000))     /* 500 ms */
            continue;

        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_HI, mbd->cpu.cores[i].id << 24);
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_LO, ((AP_BOOTSTRAP_CODE >> 12) & 0xFF) | (6 << 8) | (1 << 14));

        arch_timer_delay(200);

        if(ap_check(i, 3000000))    /* 3 secs */
            continue;

        kprintf("x86-smp: Failed to start CPU #%d\n", mbd->cpu.cores[i].id);
    }

    smp_main(1);
}
