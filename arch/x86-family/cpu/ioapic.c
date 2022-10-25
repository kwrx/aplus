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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/ipc.h>
#include <aplus/hal.h>

#include <arch/x86/cpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/intr.h>
#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>


ioapic_t ioapic[X86_IOAPIC_MAX];


static inline void ioapic_write(uintptr_t address, const uint32_t offset, const uint32_t value) {
    mmio_w32(address, offset & 0xFF);
    mmio_w32(address + 0x10, value);
}

static inline uint32_t ioapic_read(uintptr_t address, const uint32_t offset) {
    mmio_w32(address, offset & 0xFF);
    return mmio_r32(address + 0x10);
}


void ioapic_map_irq(irq_t source, irq_t irq, cpuid_t cpu) {
    
    int i;
    for(i = 0; i < X86_IOAPIC_MAX; i++) {
       
        if(!ioapic[i].address)
            continue;

        if(
            (source >= (ioapic[i].gsi_base)) &&
            (source <= (ioapic[i].gsi_base + ioapic[i].gsi_max - 1))
        ) {

            __lock(&ioapic[i].lock, {
                
                uint64_t d = 0;
                d |= (0x20 + irq) & 0xFF;
                d |= (uint64_t) cpu << 56;

                ioapic_write(ioapic[i].address, X86_IOAPIC_IOAPICREDTBL(source), d & 0xFFFFFFFF);
                ioapic_write(ioapic[i].address, X86_IOAPIC_IOAPICREDTBL(source) + 1, (d >> 32) & 0xFFFFFFFF);
            
            });

            return;
        }
    }

    kpanicf("x86-apic: Source Interrupt #%d not managed by any I/O APIC\n", source);
}

void ioapic_unmap_irq(irq_t source) {

    if(unlikely(source == 0))   /* LVT0, APIC TIMER */
        return; //kpanicf("x86-ioapic: attempting to unmap irq#0 (LVT0, APIC TIMER)");


    int i;
    for(i = 0; i < X86_IOAPIC_MAX; i++) {
        
        if(!ioapic[i].address)
            continue;

        if(
            (source >= (ioapic[i].gsi_base)) &&
            (source <= (ioapic[i].gsi_base + ioapic[i].gsi_max - 1))
        ) {

            __lock(&ioapic[i].lock, {

                uint64_t d = 0;
                d |= (1 << 16);

                ioapic_write(ioapic[i].address, X86_IOAPIC_IOAPICREDTBL(source), d & 0xFFFFFFFF);
                ioapic_write(ioapic[i].address, X86_IOAPIC_IOAPICREDTBL(source) + 1, (d >> 32) & 0xFFFFFFFF);

            });

            return;
        }
    }

    kpanicf("x86-apic: Source Interrupt #%d not managed by any I/O APIC\n", source);
}


void ioapic_enable(void) {
    
    int i;
    for(i = 0; i < X86_IOAPIC_MAX; i++) {
        
        if(!ioapic[i].address)
            continue;


        if(ioapic[i].address < ((core->memory.phys_upper + core->memory.phys_lower) * 1024))
            pmm_claim_area (
                ioapic[i].address,
                ioapic[i].address + PML1_PAGESIZE
            );


        arch_vmm_map (
            &core->bsp.address_space,
            ioapic[i].address,
            ioapic[i].address,
            PML1_PAGESIZE,
            
            ARCH_VMM_MAP_RDWR       |
            ARCH_VMM_MAP_UNCACHED   |
            ARCH_VMM_MAP_NOEXEC     |
            ARCH_VMM_MAP_FIXED
        );


        ioapic_write (
            ioapic[i].address, X86_IOAPIC_IOAPICID, (i & 0xF) << 24
        );

        ioapic[i].gsi_max = ioapic_read(ioapic[i].address, X86_IOAPIC_IOAPICVER) >> 16;
        ioapic[i].gsi_max &= 0xFF;


        spinlock_init_with_flags(&ioapic[i].lock, SPINLOCK_FLAGS_CPU_OWNER);
        

        int j;
        for(j = ioapic[i].gsi_base; j < ioapic[i].gsi_max; j++)
            ioapic_unmap_irq(j);


#if DEBUG_LEVEL_INFO
        kprintf("x86-apic: I/O APIC #%d initialized [base(0x%lX), gsi(%d-%d)]\n", i, ioapic[i].address, ioapic[i].gsi_base, ioapic[i].gsi_max);
#endif

    }
}