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
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>

#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>
#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>



extern ioapic_t ioapic[];
static uint32_t timer_ticks;
static int x2apic;
static uintptr_t lapic_address = X86_APIC_BASE_ADDR;
static uint32_t isa_gsi[16];
static uint64_t isa_flags[16];
static uint8_t isa_overridden[16];


static uint32_t apic_bootstrap_archid(void) {

    long max_leaf, bx, cx, dx;
    x86_cpuid(0, &max_leaf, &bx, &cx, &dx);

    const long topology_leaves[] = {0x1F, 0x0B};

    for (size_t i = 0; i < sizeof(topology_leaves) / sizeof(topology_leaves[0]); i++) {

        if (max_leaf < topology_leaves[i])
            continue;

        cx = 0;
        x86_cpuid_extended(topology_leaves[i], &max_leaf, &bx, &cx, &dx);

        if (bx)
            return (uint32_t)dx;
    }

    x86_cpuid(1, &max_leaf, &bx, &cx, &dx);
    return ((uint32_t)bx >> 24);
}

static void apic_validate_address(uint64_t address) {

    long max_leaf, bx, cx, dx;
    x86_cpuid(0x80000000, &max_leaf, &bx, &cx, &dx);

    uint8_t physical_address_bits = 36;

    if ((uint32_t)max_leaf >= 0x80000008) {
        x86_cpuid(0x80000008, &max_leaf, &bx, &cx, &dx);

        if ((max_leaf & 0xFF) >= 32 && (max_leaf & 0xFF) <= 64)
            physical_address_bits = max_leaf & 0xFF;
    }

    uint64_t physical_address_mask = physical_address_bits >= 64 ? UINT64_MAX : ((1ULL << physical_address_bits) - 1);

    if (!address || (address & (PML1_PAGESIZE - 1)) || (address & ~physical_address_mask) || address > UINTPTR_MAX)
        kpanicf("x86-apic: malformed MADT Local APIC address 0x%lX\n", address);
}

static void apic_validate_entry_length(const acpi_madt_entry_t* entry, size_t expected) {

    if (entry->length != expected)
        kpanicf("x86-apic: malformed MADT entry type %d length %d, expected %zd\n", entry->type, entry->length, expected);
}

static void apic_add_cpu(uint32_t archid, uint32_t flags, uint32_t bsp_archid, uint32_t* discovered_ids, size_t* discovered_count, size_t* next_index) {

    if (!(flags & (X86_MADT_CPU_ENABLED | X86_MADT_CPU_ONLINE_CAPABLE)))
        return;

    for (size_t i = 0; i < *discovered_count; i++) {
        if (discovered_ids[i] == archid) {
            kprintf("x86-apic: WARN! ignoring duplicate CPU APIC ID %d\n", archid);
            return;
        }
    }

    if (!x2apic && archid > UINT8_MAX) {
        kprintf("x86-apic: WARN! ignoring CPU APIC ID %d, unavailable in xAPIC mode\n", archid);
        return;
    }

    size_t index;

    if (archid == bsp_archid) {
        index = SMP_CPU_BOOTSTRAP_ID;
    } else {
        if (*next_index >= SMP_CPU_MAX) {
            kprintf("x86-apic: WARN! ignoring CPU APIC ID %d, SMP_CPU_MAX reached\n", archid);
            return;
        }

        index = (*next_index)++;
    }

    discovered_ids[(*discovered_count)++] = archid;

    core->cpu.cores[index].id     = index;
    core->cpu.cores[index].node   = 0;
    core->cpu.cores[index].archid = archid;
    core->cpu.cores[index].flags |= SMP_CPU_FLAGS_AVAILABLE;

    core->cpu.max_cores = *discovered_count;

#if DEBUG_LEVEL_TRACE
    kprintf("x86-apic: CPU #%zd discovered [apic-id(%d), flags(0x%X)]\n", index, archid, flags);
#endif
}

static void apic_add_ioapic(const acpi_madt_ioapic_t* entry, size_t* count) {

    if (!entry->address || (entry->address & (PML1_PAGESIZE - 1)))
        kpanicf("x86-apic: malformed MADT I/O APIC address 0x%X\n", entry->address);

    for (size_t i = 0; i < *count; i++) {
        if (ioapic[i].id == entry->id || ioapic[i].address == entry->address || ioapic[i].gsi_base == entry->gsi_base) {
            kprintf("x86-apic: WARN! ignoring duplicate I/O APIC id(%d) address(0x%X) gsi(%d)\n", entry->id, entry->address, entry->gsi_base);
            return;
        }
    }

    if (*count >= X86_IOAPIC_MAX)
        kpanicf("x86-apic: more than %d I/O APICs found\n", X86_IOAPIC_MAX);

    ioapic[*count].id       = entry->id;
    ioapic[*count].address  = entry->address;
    ioapic[*count].gsi_base = entry->gsi_base;
    (*count)++;
}

static uint64_t apic_interrupt_override_flags(uint16_t flags) {

    if (flags & ~(X86_MADT_INTERRUPT_POLARITY_MASK | X86_MADT_INTERRUPT_TRIGGER_MASK))
        kpanicf("x86-apic: malformed MADT interrupt override flags 0x%X\n", flags);

    uint64_t result = 0;

    switch (flags & X86_MADT_INTERRUPT_POLARITY_MASK) {
        case X86_MADT_INTERRUPT_POLARITY_CONFORMING:
        case X86_MADT_INTERRUPT_POLARITY_ACTIVE_HIGH:
            result |= X86_IOAPIC_REDTTBL_FLAG_POLARITY_ACTIVE_HIGH;
            break;
        case X86_MADT_INTERRUPT_POLARITY_ACTIVE_LOW:
            result |= X86_IOAPIC_REDTTBL_FLAG_POLARITY_ACTIVE_LOW;
            break;
        default:
            kpanicf("x86-apic: malformed MADT interrupt override polarity 0x%X\n", flags);
    }

    switch (flags & X86_MADT_INTERRUPT_TRIGGER_MASK) {
        case X86_MADT_INTERRUPT_TRIGGER_CONFORMING:
        case X86_MADT_INTERRUPT_TRIGGER_EDGE:
            result |= X86_IOAPIC_REDTTBL_FLAG_TRIGGER_MODE_EDGE;
            break;
        case X86_MADT_INTERRUPT_TRIGGER_LEVEL:
            result |= X86_IOAPIC_REDTTBL_FLAG_TRIGGER_MODE_LEVEL;
            break;
        default:
            kpanicf("x86-apic: malformed MADT interrupt override trigger 0x%X\n", flags);
    }

    return result;
}

static void apic_add_interrupt_override(const acpi_madt_interrupt_override_t* entry) {

    if (entry->bus != 0 || entry->source >= 16) {
        kprintf("x86-apic: WARN! ignoring non-ISA interrupt override bus(%d) source(%d)\n", entry->bus, entry->source);
        return;
    }

    uint64_t flags = apic_interrupt_override_flags(entry->flags);

    if (isa_overridden[entry->source]) {
        if (isa_gsi[entry->source] != entry->gsi || isa_flags[entry->source] != flags)
            kpanicf("x86-apic: conflicting MADT interrupt overrides for ISA IRQ %d\n", entry->source);

        kprintf("x86-apic: WARN! ignoring duplicate interrupt override for ISA IRQ %d\n", entry->source);
        return;
    }

    isa_gsi[entry->source]        = entry->gsi;
    isa_flags[entry->source]      = flags;
    isa_overridden[entry->source] = 1;

#if DEBUG_LEVEL_INFO
    kprintf("x86-apic: ISA IRQ %d override [gsi(%d), flags(0x%lX)]\n", entry->source, entry->gsi, flags);
#endif
}


__percpu void apic_enable(void) {

    uint64_t msr = x86_rdmsr(X86_APIC_BASE_MSR);

    uint64_t base = lapic_address | (msr & ~X86_APIC_BASE_MASK & ~(X86_APIC_MSR_EN | X86_APIC_MSR_EXTD));

    if (x2apic && !(msr & X86_APIC_MSR_EN))
        x86_wrmsr(X86_APIC_BASE_MSR, base | X86_APIC_MSR_EN);

    if (!x2apic && (msr & X86_APIC_MSR_EXTD))
        x86_wrmsr(X86_APIC_BASE_MSR, base);

    x86_wrmsr(X86_APIC_BASE_MSR, base | X86_APIC_MSR_EN | (x2apic ? X86_APIC_MSR_EXTD : 0));


    if (x2apic) {

        x86_wrmsr(X86_X2APIC_REG_LVT_TIMER, (1 << 16));
        x86_wrmsr(X86_X2APIC_REG_LVT_THERMAL, (1 << 16));
        x86_wrmsr(X86_X2APIC_REG_LVT_PERFMON, (1 << 16));
        x86_wrmsr(X86_X2APIC_REG_LVT_LINT0, (1 << 16));
        x86_wrmsr(X86_X2APIC_REG_LVT_LINT1, (1 << 16));

        x86_wrmsr(X86_X2APIC_REG_TASK_PRIO, 0);
        x86_wrmsr(X86_X2APIC_REG_SPURIOUS, 0x1FF);

    } else {

        mmio_w32(lapic_address + X86_APIC_REG_LVT_TIMER, (1 << 16));
        mmio_w32(lapic_address + X86_APIC_REG_LVT_THERMAL, (1 << 16));
        mmio_w32(lapic_address + X86_APIC_REG_LVT_PERFMON, (1 << 16));
        mmio_w32(lapic_address + X86_APIC_REG_LVT_LINT0, (1 << 16));
        mmio_w32(lapic_address + X86_APIC_REG_LVT_LINT1, (1 << 16));

        mmio_w32(lapic_address + X86_APIC_REG_DFR, 0xFFFFFFFF);
        mmio_w32(lapic_address + X86_APIC_REG_TASK_PRIO, 0);
        mmio_w32(lapic_address + X86_APIC_REG_SPURIOUS, 0x1FF);
    }



#if DEBUG_LEVEL_WARN
    {

        long a, b, c, d;
        x86_cpuid(6, &a, &b, &c, &d);

        if (!(a & (1 << 2)))
            kprintf("x86-apic: WARN! APIC timer may temporarily stop while the processor is in deep C-states: %ld\n", a);
    }
#endif



    if (current_cpu->id == SMP_CPU_BOOTSTRAP_ID) {


        //? Synchronize timer clock

        uint64_t ts, t0;

        ts = t0 = arch_timer_generic_getns();

        while ((t0 = arch_timer_generic_getns()) == ts)
            ;



        if (x2apic) {

            x86_wrmsr(X86_X2APIC_REG_TMR_DIV, 3);
            x86_wrmsr(X86_X2APIC_REG_TMR_ICNT, 0xFFFFFFFF);

        } else {

            mmio_w32(lapic_address + X86_APIC_REG_TMR_DIV, 3);
            mmio_w32(lapic_address + X86_APIC_REG_TMR_ICNT, 0xFFFFFFFF);
        }


        //? 0.001s every interrupt

        while ((arch_timer_generic_getns() - t0) < (TASK_SCHEDULER_PERIOD_NS << 4))
            ;



        uint32_t ticks = 0xFFFFFFFF;

        if (x2apic)
            ticks -= x86_rdmsr(X86_X2APIC_REG_TMR_CCNT);
        else
            ticks -= mmio_r32(lapic_address + X86_APIC_REG_TMR_CCNT);


        timer_ticks = ticks >> 4;
    }


    DEBUG_ASSERT(timer_ticks);


    apic_timer_reset(1);


#if DEBUG_LEVEL_INFO
    kprintf("x86-apic: Local APIC #%d initialized [base(0x%lX), ticks(%d), x2apic(%d)]\n", apic_get_id(), lapic_address, timer_ticks, x2apic);
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

        mmio_w32(lapic_address + X86_APIC_REG_LVT_TIMER, (0 << 17) | 32);
        mmio_w32(lapic_address + X86_APIC_REG_TMR_DIV, 3);
        mmio_w32(lapic_address + X86_APIC_REG_TMR_ICNT, timer_ticks * multiplier);
    }
}


void apic_eoi(void) {

    if (x2apic)
        x86_wrmsr(X86_X2APIC_REG_EOI, 0);
    else
        mmio_w32(lapic_address + X86_APIC_REG_EOI, 0);
}


uint32_t apic_get_id(void) {

    if (x2apic)
        return (x86_rdmsr(X86_X2APIC_REG_ID) & 0xFFFFFFFF);
    else
        return (mmio_r32(lapic_address + X86_APIC_REG_ID) >> 24);
}

int apic_is_x2apic(void) {
    return x2apic;
}

uintptr_t apic_get_base(void) {
    return lapic_address;
}

void apic_get_isa_irq(irq_t irq, uint32_t* gsi, uint64_t* flags) {

    DEBUG_ASSERT(irq < 16);
    DEBUG_ASSERT(gsi);

    *gsi = isa_gsi[irq];

    if (flags)
        *flags = isa_flags[irq];
}


void apic_init(void) {

    memset(ioapic, 0, sizeof(ioapic_t) * X86_IOAPIC_MAX);

    for (size_t i = 0; i < 16; i++) {
        isa_gsi[i]   = i;
        isa_flags[i] = X86_IOAPIC_REDTTBL_FLAG_TRIGGER_MODE_EDGE | X86_IOAPIC_REDTTBL_FLAG_POLARITY_ACTIVE_HIGH;
    }
    memset(isa_overridden, 0, sizeof(isa_overridden));


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



    if (!boot_cpu_has(X86_FEATURE_APIC))
        kpanicf("x86-apic: APIC not supported!\n");

    x2apic = 0;

#if !defined(CONFIG_X86_X2APIC_FORCE_DISABLED)
    if (boot_cpu_has(X86_FEATURE_X2APIC))
        x2apic = 1;
#endif

    acpi_sdt_t* sdt;
    if (acpi_find(&sdt, "APIC") != 0)
        kpanicf("x86-apic: APIC not found in ACPI tables\n");

    if (sdt->length < sizeof(acpi_sdt_t) + sizeof(acpi_madt_t))
        kpanicf("x86-apic: malformed MADT table length %d\n", sdt->length);

    acpi_madt_t* madt = (acpi_madt_t*)((uint8_t*)sdt + sizeof(acpi_sdt_t));

    apic_validate_address(madt->lapic_address);
    lapic_address = madt->lapic_address;

    uint32_t bsp_archid = apic_bootstrap_archid();
    uint32_t discovered_ids[SMP_CPU_MAX];
    size_t discovered_count = 0;
    size_t next_cpu_index    = 1;
    size_t ioapic_count      = 0;
    int address_overridden   = 0;

    uint8_t* entry_ptr = madt->entries;
    uint8_t* table_end = (uint8_t*)sdt + sdt->length;

    while (entry_ptr < table_end) {

        if ((size_t)(table_end - entry_ptr) < sizeof(acpi_madt_entry_t))
            kpanicf("x86-apic: malformed MADT entry header at offset %zd\n", entry_ptr - (uint8_t*)sdt);

        acpi_madt_entry_t* header = (acpi_madt_entry_t*)entry_ptr;

        if (header->length < sizeof(acpi_madt_entry_t) || header->length > (size_t)(table_end - entry_ptr))
            kpanicf("x86-apic: malformed MADT entry type %d length %d\n", header->type, header->length);

        switch (header->type) {
            case X86_MADT_ENTRY_LAPIC: {
                apic_validate_entry_length(header, sizeof(acpi_madt_lapic_t));
                acpi_madt_lapic_t* entry = (acpi_madt_lapic_t*)header;
                apic_add_cpu(entry->apic_id, entry->flags, bsp_archid, discovered_ids, &discovered_count, &next_cpu_index);
                break;
            }

            case X86_MADT_ENTRY_IOAPIC: {
                apic_validate_entry_length(header, sizeof(acpi_madt_ioapic_t));
                apic_add_ioapic((acpi_madt_ioapic_t*)header, &ioapic_count);
                break;
            }

            case X86_MADT_ENTRY_INTERRUPT_OVERRIDE: {
                apic_validate_entry_length(header, sizeof(acpi_madt_interrupt_override_t));
                apic_add_interrupt_override((acpi_madt_interrupt_override_t*)header);
                break;
            }

            case X86_MADT_ENTRY_LAPIC_NMI:
                apic_validate_entry_length(header, sizeof(acpi_madt_lapic_nmi_t));
                break;

            case X86_MADT_ENTRY_LAPIC_ADDRESS_OVERRIDE: {
                apic_validate_entry_length(header, sizeof(acpi_madt_lapic_address_override_t));
                acpi_madt_lapic_address_override_t* entry = (acpi_madt_lapic_address_override_t*)header;

                if (address_overridden)
                    kpanicf("x86-apic: multiple MADT Local APIC address overrides\n");

                apic_validate_address(entry->address);
                lapic_address      = entry->address;
                address_overridden = 1;
                break;
            }

            case X86_MADT_ENTRY_X2APIC: {
                apic_validate_entry_length(header, sizeof(acpi_madt_x2apic_t));
                acpi_madt_x2apic_t* entry = (acpi_madt_x2apic_t*)header;
                apic_add_cpu(entry->x2apic_id, entry->flags, bsp_archid, discovered_ids, &discovered_count, &next_cpu_index);
                break;
            }

            default:
                kprintf("x86-apic: WARN! ignoring unknown MADT entry type %d length %d\n", header->type, header->length);
                break;
        }

        entry_ptr += header->length;
    }

    int bsp_found = 0;
    for (size_t i = 0; i < discovered_count; i++) {
        if (discovered_ids[i] == bsp_archid) {
            bsp_found = 1;
            break;
        }
    }

    if (!bsp_found)
        kpanicf("x86-apic: bootstrap CPU APIC ID %d not found in MADT\n", bsp_archid);

#if DEBUG_LEVEL_INFO
    kprintf("x86-apic: discovered %zd CPUs, %zd I/O APICs [lapic-base(0x%lX), x2apic(%d)]\n", discovered_count, ioapic_count, lapic_address, x2apic);
#endif

    if (lapic_address < ((core->memory.phys_upper + core->memory.phys_lower) * 1024)) {

        pmm_claim_area(lapic_address, PML1_PAGESIZE);
    }


    if (!x2apic) {

        arch_vmm_map(&core->bsp.address_space, lapic_address, lapic_address, PML1_PAGESIZE,

                     ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_UNCACHED | ARCH_VMM_MAP_NOEXEC | ARCH_VMM_MAP_FIXED);
    }


    ioapic_enable();
    apic_enable();


    __asm__ __volatile__("sti");
}
