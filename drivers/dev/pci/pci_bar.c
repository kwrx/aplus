/*
 * Author:
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

#include <stdbool.h>
#include <stdint.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>

#include <arch/x86/asm.h>

#include <dev/pci.h>


#define PCI_MMIO_START    0x100000000ULL
#define PCI_MMIO_MAPPINGS 256

struct pci_mmio_mapping {
    uintptr_t phys;
    uintptr_t length;
    uintptr_t virt;
};

static struct pci_mmio_mapping pci_mmio_mappings[PCI_MMIO_MAPPINGS];
static size_t pci_mmio_count  = 0;
static uintptr_t pci_mmio_next = PCI_MMIO_START;
static spinlock_t pci_mmio_lock = SPINLOCK_INIT;


static bool pci_bar_probe(pcidev_t device, int field, uintptr_t* base, uintptr_t* size) {

    uint32_t lo = pci_read(device, field, 4);

    if (lo & 1)
        return false;

    pci_write(device, field, 4, 0xFFFFFFFF);
    uint64_t mask = pci_read(device, field, 4) & PCI_BAR_MM_MASK;
    pci_write(device, field, 4, lo);

    uint64_t addr = lo & PCI_BAR_MM_MASK;

    if ((lo & 6) == 4) {

        uint32_t hi = pci_read(device, field + 4, 4);

        pci_write(device, field + 4, 4, 0xFFFFFFFF);
        mask |= (uint64_t)pci_read(device, field + 4, 4) << 32;
        pci_write(device, field + 4, 4, hi);

        addr |= (uint64_t)hi << 32;

    } else {

        if (!mask)
            return false;

        mask |= 0xFFFFFFFF00000000ULL;
    }

    if (!addr || !mask)
        return false;

    *base = addr;
    *size = ~mask + 1;

    return true;
}


uintptr_t pci_map_bar(pcidev_t device, int field, uintptr_t* size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(field >= PCI_BAR0 && field <= PCI_BAR5);

    uintptr_t base;
    uintptr_t length;

    if (!pci_bar_probe(device, field, &base, &length))
        return 0;

    uintptr_t phys = base & ~((uintptr_t)PML1_PAGESIZE - 1);
    uintptr_t span = (base + length + PML1_PAGESIZE - 1 - phys) & ~((uintptr_t)PML1_PAGESIZE - 1);
    uintptr_t virt = 0;

    scoped_lock(&pci_mmio_lock) {

        for (size_t i = 0; i < pci_mmio_count && !virt; i++) {

            struct pci_mmio_mapping* m = &pci_mmio_mappings[i];

            if (phys >= m->phys && phys + span <= m->phys + m->length)
                virt = m->virt + (phys - m->phys);
        }

        if (!virt) {

            if (pci_mmio_count == PCI_MMIO_MAPPINGS || span > KERNEL_MMIO_SIZE - pci_mmio_next)
                return 0;

            uintptr_t v = KERNEL_MMIO_AREA + pci_mmio_next;

            pci_mmio_next += span;

            if (arch_vmm_map(&core->bsp.address_space, v, phys, span, ARCH_VMM_MAP_FIXED | ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_UNCACHED | ARCH_VMM_MAP_WRITE_THROUGH | ARCH_VMM_MAP_NOEXEC) == ARCH_VMM_MAP_FAILED)
                return 0;

            pci_mmio_mappings[pci_mmio_count++] = (struct pci_mmio_mapping){phys, span, v};

            virt = v;
        }
    }

    if (size)
        *size = length;

#if DEBUG_LEVEL_TRACE
    kprintf("pci: mapped device(%d) field(%d) from %p:%x to %p:%x\n", device, field, base, length, virt + (base - phys), length);
#endif

    return virt + (base - phys);
}
