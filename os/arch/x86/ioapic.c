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
#include <arch/x86/mm.h>
#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>
#include <arch/x86/cpu.h>
#include <string.h>


ioapic_t ioapic[X86_IOAPIC_MAX];


static inline void ioapic_write(uintptr_t address, uint32_t offset, uint32_t value) {
    mmio_w32(address, offset & 0xFF);
    mmio_w32(address + 0x10, value);
}

static inline uint32_t ioapic_read(uintptr_t address, uint32_t offset) {
    mmio_w32(address, offset & 0xFF);
    return mmio_r32(address + 0x10);
}


void ioapic_map_irq(uint8_t source, uint8_t irq, uint8_t cpu) {
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

    kpanic("x86-apic: Source Interrupt #%d not managed by any I/O APIC", source);
}

void ioapic_unmap_irq(uint8_t source) {
    if(unlikely(source == 0))   /* LVT0, APIC TIMER */
        return;

    
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

    kpanic("x86-apic: Source Interrupt #%d not managed by any I/O APIC", source);
}


void ioapic_enable(void) {
    
    int i;
    for(i = 0; i < X86_IOAPIC_MAX; i++) {
        if(!ioapic[i].address)
            continue;

    
        x86_map_page (
            (x86_page_t*) (x86_get_cr3() + CONFIG_KERNEL_BASE),
            (ioapic[i].address & ~(PAGE_SIZE - 1)), 
            (ioapic[i].address & ~(PAGE_SIZE - 1)) >> 12, 
            X86_MMU_PG_P | X86_MMU_PG_RW | X86_MMU_PG_CD
        );

        pmm_claim (
            ioapic[i].address,
            ioapic[i].address + PAGE_SIZE - 1
        );

        ioapic_write ( /* FIXME: KVM Internal error on x86_64 */
            ioapic[i].address, X86_IOAPIC_IOAPICID, (i & 0xF) << 24
        );

        ioapic[i].gsi_max = ioapic_read(ioapic[i].address, X86_IOAPIC_IOAPICVER) >> 16;
        ioapic[i].gsi_max &= 0xFF;

        spinlock_init(&ioapic[i].lock);
        

        int j;
        for(j = ioapic[i].gsi_base; j < ioapic[i].gsi_max; j++)
            ioapic_unmap_irq(j);


        kprintf("x86-apic: I/O APIC #%d initialized [base(%p), gsi(%d-%d)]\n", i, ioapic[i].address, ioapic[i].gsi_base, ioapic[i].gsi_max);
    }
}