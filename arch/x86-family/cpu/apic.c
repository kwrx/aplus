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
#include <time.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/ipc.h>
#include <aplus/smp.h>
#include <aplus/hal.h>

#include <arch/x86/cpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/intr.h>
#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>



extern ioapic_t ioapic[];
static uint32_t timer_ticks;
static int x2apic;


__percpu
void apic_enable(void) {

    uint64_t msr = x86_rdmsr (
        X86_APIC_BASE_MSR
    );

    x86_wrmsr (
        X86_APIC_BASE_MSR, 
        X86_APIC_BASE_ADDR | X86_APIC_MSR_EN | (x2apic ? X86_APIC_MSR_EXTD : 0) | (msr & 0x3FF)
    );


    if (x2apic) {

        x86_wrmsr(X86_X2APIC_REG_LVT_TIMER, (1 << 16));
        x86_wrmsr(X86_X2APIC_REG_LVT_THERMAL, (1 << 16));
        x86_wrmsr(X86_X2APIC_REG_LVT_PERFMON, (1 << 16));
        x86_wrmsr(X86_X2APIC_REG_LVT_LINT0, (1 << 16));
        x86_wrmsr(X86_X2APIC_REG_LVT_LINT1, (1 << 16));
        
        x86_wrmsr(X86_X2APIC_REG_TASK_PRIO, 0);
        x86_wrmsr(X86_X2APIC_REG_SPURIOUS, 0x1FF);

    } else {

        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_LVT_TIMER, (1 << 16));
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_LVT_THERMAL, (1 << 16));
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_LVT_PERFMON, (1 << 16));
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_LVT_LINT0, (1 << 16));
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_LVT_LINT1, (1 << 16));
        
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_DFR, 0xFFFFFFFF);
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_TASK_PRIO, 0);
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_SPURIOUS, 0x1FF);

    }



#if defined(DEBUG) && DEBUG_LEVEL >= 1
    {
        long a, b, c, d;
        x86_cpuid(6, &a, &b, &c, &d);

        if(!(a & (1 << 2)))
            kprintf("x86-apic: WARN! APIC timer may temporarily stop while the processor is in deep C-states\n", a);
    }
#endif



    if(current_cpu->id == SMP_CPU_BOOTSTRAP_ID) {


        //? Synchronize timer clock

        uint64_t ts, t0;
        
        ts =
        t0 = arch_timer_generic_getns();

        while((t0 = arch_timer_generic_getns()) == ts)
            ;

        

        if (x2apic) {

            x86_wrmsr(X86_X2APIC_REG_TMR_DIV, 3);
            x86_wrmsr(X86_X2APIC_REG_TMR_ICNT, 0xFFFFFFFF);

        } else {
            
            mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_TMR_DIV, 3);
            mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_TMR_ICNT, 0xFFFFFFFF);
        
        }


        //? 0.001s every interrupt

        while((arch_timer_generic_getns() - t0) < (TASK_SCHEDULER_PERIOD_NS << 4))
            ;
        


        uint32_t ticks = 0xFFFFFFFF;
        
        if(x2apic)
            ticks -= x86_rdmsr(X86_X2APIC_REG_TMR_CCNT);
        else
            ticks -= mmio_r32(X86_APIC_BASE_ADDR + X86_APIC_REG_TMR_CCNT);

        
        timer_ticks = ticks >> 4;

    }


    DEBUG_ASSERT(timer_ticks);


    apic_timer_reset(1);


#if defined(DEBUG) && DEBUG_LEVEL >= 0
    kprintf("x86-apic: Local APIC #%d initialized [base(%p), ticks(%d), x2apic(%d)]\n", apic_get_id(), X86_APIC_BASE_ADDR, timer_ticks, x2apic);
#endif

}



void apic_timer_reset(uint32_t multiplier) {

    DEBUG_ASSERT(timer_ticks);
    DEBUG_ASSERT(multiplier);


    if (x2apic) {
        
        x86_wrmsr(X86_X2APIC_REG_LVT_TIMER, (0 << 17) | 32);
        x86_wrmsr(X86_X2APIC_REG_TMR_DIV, 3);
        x86_wrmsr(X86_X2APIC_REG_TMR_ICNT, timer_ticks * multiplier);

    } else {

        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_LVT_TIMER, (0 << 17) | 32);
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_TMR_DIV, 3);
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_TMR_ICNT, timer_ticks * multiplier);

    }

}


void apic_eoi(void) {

    if (x2apic)
        x86_wrmsr(X86_X2APIC_REG_EOI, 0);
    else
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_EOI, 0);

}


uint32_t apic_get_id(void) {

    if (x2apic)
        return (x86_rdmsr(X86_X2APIC_REG_ID) & 0xFFFFFFFF);
    else
        return (mmio_r32(X86_APIC_BASE_ADDR + X86_APIC_REG_ID) >> 24);

}

int apic_is_x2apic(void) {
    return x2apic;
}


void apic_init(void) {

    memset(ioapic, 0, sizeof(ioapic_t) * X86_IOAPIC_MAX);

    
    //* Disable PIC
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);



    if(!boot_cpu_has(X86_FEATURE_APIC))
        kpanicf("x86-apic: APIC not supported!\n");


    acpi_sdt_t* sdt;
    if(acpi_find(&sdt, "APIC") != 0)
        kpanicf("x86-apic: APIC not found in ACPI tables\n");


    
    acpi_madt_t* madt;

    if(acpi_is_extended())
        madt = (acpi_madt_t*) &sdt->xtables;
    else
        madt = (acpi_madt_t*) &sdt->tables;

    DEBUG_ASSERT(madt);
    DEBUG_ASSERT(madt->lapic_address == X86_APIC_BASE_ADDR);



    uint8_t* p = madt->entries;

    for(int i = 0x2C; i < sdt->length; ) {

        switch(*p) {

            case X86_MADT_ENTRY_LAPIC:

                core->cpu.max_cores++;
                core->cpu.cores[p[2]].id = p[2];
                core->cpu.cores[p[2]].node = 0ULL;
                core->cpu.cores[p[2]].archid = p[3];
                
                if(p[2] != SMP_CPU_BOOTSTRAP_ID)
                    core->cpu.cores[p[2]].flags = 0ULL;
                
                if(*(uint32_t*) &p[4] & (1 << 0))
                    core->cpu.cores[p[2]].flags |= SMP_CPU_FLAGS_AVAILABLE;
                    
                break;

            case X86_MADT_ENTRY_IOAPIC:
                
                ioapic[p[2]].address = *(uint32_t*) &p[4];
                ioapic[p[2]].gsi_base = *(uint32_t*) &p[8];
                break;

            case X86_MADT_ENTRY_INTERRUPT:
                break;

            case X86_MADT_ENTRY_NMI:
                break;

            case X86_MADT_ENTRY_LAPIC64:

                kpanicf("x86-apic: X86_MADT_ENTRY_LAPIC64 not yet supported in x86-64\n");
                break;

            default:
                HALT_ON(0);
                break;

        }


#if defined(DEBUG)

        switch(*p) {

            case X86_MADT_ENTRY_LAPIC:
                    
                kprintf("x86-apic: X86_MADT_ENTRY_LAPIC: cpu (%d) id(%d) flags(%p)\n",
                    p[2],
                    p[3],
                    *(uint32_t*) &p[4]
                );
                break;

            case X86_MADT_ENTRY_IOAPIC:

                kprintf("x86-apic: X86_MADT_ENTRY_IOAPIC: id(%d) address(%p) gsi(%d)\n",
                    p[2],
                    *(uint32_t*) &p[4],
                    *(uint32_t*) &p[8]
                );
                break;

            case X86_MADT_ENTRY_INTERRUPT:

                kprintf("x86-apic: X86_MADT_ENTRY_INTERRUPT: bus(%d) irq(%d) gsi(%d) flags(%p)\n",
                    p[2],
                    p[3],
                    *(uint32_t*) &p[4],
                    *(uint16_t*) &p[8]
                );
                break;

            case X86_MADT_ENTRY_NMI:

                kprintf("x86-apic: X86_MADT_ENTRY_NMI: id(%d) flags(%p) lint(%d)\n",
                    p[2],
                    *(uint16_t*) &p[3],
                    p[5]
                );
                break;

            case X86_MADT_ENTRY_LAPIC64:
            
                kprintf("x86-apic: X86_MADT_ENTRY_LAPIC64: address(%p)\n",
                    *(uint64_t*) &p[4]
                );
                break;
            
            default:
                break;

        }

#endif

        i += p[1];
        p += p[1];

    }



    x2apic = 0;

#if !defined(CONFIG_X86_X2APIC_FORCE_DISABLED)
    if(boot_cpu_has(X86_FEATURE_X2APIC))
        x2apic = 1;
#endif



    if(X86_APIC_BASE_ADDR < ((core->memory.phys_upper + core->memory.phys_lower) * 1024))
        pmm_claim_area (
            X86_APIC_BASE_ADDR,
            X86_APIC_BASE_ADDR + PML1_PAGESIZE
        );


    if(!x2apic) {
        
        arch_vmm_map (
            &core->bsp.address_space,
            X86_APIC_BASE_ADDR,
            X86_APIC_BASE_ADDR,
            PML1_PAGESIZE,
            
            ARCH_VMM_MAP_RDWR       |
            ARCH_VMM_MAP_UNCACHED   |
            ARCH_VMM_MAP_NOEXEC     |
            ARCH_VMM_MAP_FIXED
        );

    }


    ioapic_enable();
    apic_enable();


    __asm__ __volatile__("sti");

}