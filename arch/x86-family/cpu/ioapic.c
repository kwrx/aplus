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
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>

#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>
#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>


ioapic_t ioapic[X86_IOAPIC_MAX] = {0};


static inline void ioapic_write(uintptr_t address, const uint32_t offset, const uint32_t value) {
    mmio_w32(address, offset & 0xFF);
    mmio_w32(address + 0x10, value);
}

static inline uint32_t ioapic_read(uintptr_t address, const uint32_t offset) {
    mmio_w32(address, offset & 0xFF);
    return mmio_r32(address + 0x10);
}


void ioapic_map_irq(uint32_t source, irq_t irq, cpuid_t cpu, uint64_t flags) {

    for (size_t i = 0; i < X86_IOAPIC_MAX; i++) {

        if (!ioapic[i].address)
            continue;

        if (source >= ioapic[i].gsi_base && source - ioapic[i].gsi_base < ioapic[i].gsi_count) {

            scoped_lock(&ioapic[i].lock) {

                DEBUG_ASSERT(cpu < SMP_CPU_MAX);

                uint64_t destination = core->cpu.cores[cpu].archid;
                if (destination > UINT8_MAX)
                    kpanicf("x86-apic: CPU APIC ID %ld cannot receive I/O APIC interrupts\n", destination);

                uint32_t index = source - ioapic[i].gsi_base;
                uint64_t d = 0;
                d |= (0x20 + irq) & 0xFF;
                d |= destination << 56;
                d |= (flags & X86_IOAPIC_REDTTBL_FLAG_MASK);

                ioapic_write(ioapic[i].address, X86_IOAPIC_IOAPICREDTBL(index) + 1, (d >> 32) & 0xFFFFFFFF);
                ioapic_write(ioapic[i].address, X86_IOAPIC_IOAPICREDTBL(index), d & 0xFFFFFFFF);
            };

            return;
        }
    }

    kpanicf("x86-apic: Source Interrupt #%u not managed by any I/O APIC\n", source);
}

void ioapic_unmap_irq(uint32_t source) {

    for (size_t i = 0; i < X86_IOAPIC_MAX; i++) {

        if (!ioapic[i].address)
            continue;

        if (source >= ioapic[i].gsi_base && source - ioapic[i].gsi_base < ioapic[i].gsi_count) {

            scoped_lock(&ioapic[i].lock) {

                uint32_t index = source - ioapic[i].gsi_base;
                uint64_t d = 0;
                d |= (1 << 16);

                ioapic_write(ioapic[i].address, X86_IOAPIC_IOAPICREDTBL(index), d & 0xFFFFFFFF);
                ioapic_write(ioapic[i].address, X86_IOAPIC_IOAPICREDTBL(index) + 1, (d >> 32) & 0xFFFFFFFF);
            };

            return;
        }
    }

    kpanicf("x86-apic: Source Interrupt #%u not managed by any I/O APIC\n", source);
}


void ioapic_enable(void) {

    for (size_t i = 0; i < X86_IOAPIC_MAX; i++) {

        if (!ioapic[i].address)
            continue;


        if (ioapic[i].address < ((core->memory.phys_upper + core->memory.phys_lower) * 1024)) {

            pmm_claim_area(ioapic[i].address, PML1_PAGESIZE);
        }


        arch_vmm_map(&core->bsp.address_space, ioapic[i].address, ioapic[i].address, PML1_PAGESIZE,

                     ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_UNCACHED | ARCH_VMM_MAP_NOEXEC | ARCH_VMM_MAP_FIXED);


        ioapic[i].gsi_count = ((ioapic_read(ioapic[i].address, X86_IOAPIC_IOAPICVER) >> 16) & 0xFF) + 1;

        uint64_t gsi_end = (uint64_t)ioapic[i].gsi_base + ioapic[i].gsi_count;
        if (gsi_end > (uint64_t)UINT32_MAX + 1)
            kpanicf("x86-apic: I/O APIC #%d has an invalid GSI range\n", ioapic[i].id);

        for (size_t j = 0; j < i; j++) {

            if (!ioapic[j].address)
                continue;

            uint64_t other_end = (uint64_t)ioapic[j].gsi_base + ioapic[j].gsi_count;
            if (ioapic[i].gsi_base < other_end && ioapic[j].gsi_base < gsi_end)
                kpanicf("x86-apic: I/O APIC #%d has an overlapping GSI range\n", ioapic[i].id);
        }


        spinlock_init_with_flags(&ioapic[i].lock, SPINLOCK_FLAGS_CPU_OWNER);


        for (uint32_t j = 0; j < ioapic[i].gsi_count; j++) {
            ioapic_write(ioapic[i].address, X86_IOAPIC_IOAPICREDTBL(j), 1 << 16);
            ioapic_write(ioapic[i].address, X86_IOAPIC_IOAPICREDTBL(j) + 1, 0);
        }


#if DEBUG_LEVEL_INFO
        kprintf("x86-apic: I/O APIC #%d initialized [base(0x%lX), gsi(%u-%u)]\n", ioapic[i].id, ioapic[i].address, ioapic[i].gsi_base, ioapic[i].gsi_base + ioapic[i].gsi_count - 1);
#endif
    }
}
