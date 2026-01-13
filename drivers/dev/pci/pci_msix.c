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

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/endian.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/module.h>
#include <aplus/vfs.h>

#include <dev/interface.h>
#include <dev/pci.h>


static void pci_msix_interrupt_handler(void* frame, irq_t irq) {

    DEBUG_ASSERT(irq > PCI_MSI_INTR_BASE - 1);
    DEBUG_ASSERT(irq < PCI_MSI_INTR_BASE + PCI_DEVICES_MAX);

    uint16_t index = irq - PCI_MSI_INTR_BASE;

    pcidev_t device = pci_dev_get_device(index);
    if (device == 0)
        return;

    pci_dev_call_handler(index, irq);
}


int pci_find_msix(pcidev_t device, pci_msix_t* mptr) {

    uintptr_t caps;
    if ((caps = pci_find_capabilities(device)) == PCI_NONE)
        return PCI_NONE;

    pci_msix_t msix;
    do {

        pci_memcpy(device, &msix.msix_pci, caps, sizeof(pci_msix_t));

        if (msix.msix_pci.pci_capid != PCI_MSIX_CAPID)
            continue;

        if (msix.msix_pci.pci_bir > 5) {
#if DEBUG_LEVEL_FATAL
            kprintf("pci-msix: FAIL! unknown pci bar #%d for device %d\n", msix.msix_pci.pci_bir, device);
#endif
            return PCI_NONE;
        }


        uintptr_t pci_bir_address = 0UL;
        size_t pci_bir_size = 0UL;

        uintptr_t pci_pba_address = 0UL;
        size_t pci_pba_size = 0UL;

#if defined(__x86_64__) || defined(__aarch64__)
        if (pci_is_64bit(device, PCI_BAR(msix.msix_pci.pci_bir))) {
            pci_bir_address = pci_read(device, PCI_BAR(msix.msix_pci.pci_bir), 8) & PCI_BAR_MM_MASK;
            pci_bir_size    = pci_bar_size(device, PCI_BAR(msix.msix_pci.pci_bir), 8);
            pci_pba_address = pci_read(device, PCI_BAR(msix.msix_pci.pci_pending_bir), 8) & PCI_BAR_MM_MASK;
            pci_pba_size    = pci_bar_size(device, PCI_BAR(msix.msix_pci.pci_pending_bir), 8);
        } else
#endif
        {
            pci_bir_address = pci_read(device, PCI_BAR(msix.msix_pci.pci_bir), 4) & PCI_BAR_MM_MASK;
            pci_bir_size    = pci_bar_size(device, PCI_BAR(msix.msix_pci.pci_bir), 4);
            pci_pba_address = pci_read(device, PCI_BAR(msix.msix_pci.pci_pending_bir), 4) & PCI_BAR_MM_MASK;
            pci_pba_size    = pci_bar_size(device, PCI_BAR(msix.msix_pci.pci_pending_bir), 4);
        }

        DEBUG_ASSERT(pci_bir_address);
        DEBUG_ASSERT(pci_bir_size);
        DEBUG_ASSERT(pci_pba_address);
        DEBUG_ASSERT(pci_pba_size);

        arch_vmm_map(&core->bsp.address_space, pci_bir_address, pci_bir_address, pci_bir_size, ARCH_VMM_MAP_FIXED | ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_UNCACHED | ARCH_VMM_MAP_NOEXEC);

        if (msix.msix_pci.pci_bir != msix.msix_pci.pci_pending_bir) {
            arch_vmm_map(&core->bsp.address_space, pci_pba_address, pci_pba_address, pci_pba_size, ARCH_VMM_MAP_FIXED | ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_UNCACHED | ARCH_VMM_MAP_NOEXEC);
        } 

        pci_bir_address += msix.msix_pci.pci_offset;
        pci_pba_address += msix.msix_pci.pci_pending_offset;

        msix.msix_cap  = caps;
        msix.msix_rows = (pci_msix_row_t volatile*)pci_bir_address;
        msix.msix_pba  = (pci_msix_pba_t volatile*)pci_pba_address;

        for (size_t i = 0; i < msix.msix_pci.pci_msgctl_table_size + 1; i++)
            pci_msix_mask(device, &msix, i);

        if (mptr)
            memcpy(mptr, &msix, sizeof(pci_msix_t));

        return 0;

    } while ((caps = msix.msix_pci.pci_capnext) != 0);

    return PCI_NONE;
}


void pci_msix_enable(pcidev_t device, pci_msix_t* msix) {
#if DEBUG_LEVEL_TRACE
    DEBUG_ASSERT(pci_read(device, msix->msix_cap, 1) == PCI_MSIX_CAPID);
#endif

    uint16_t pci_msgctl = pci_read(device, msix->msix_cap + PCI_MSIX_HDR_MSGCTL, sizeof(uint16_t));
    pci_msgctl |= PCI_MSIX_MSGCTL_ENABLE;
    pci_msgctl &= ~PCI_MSIX_MSGCTL_MASK;
    pci_write(device, msix->msix_cap + PCI_MSIX_HDR_MSGCTL, sizeof(uint16_t), pci_msgctl);
}

void pci_msix_disable(pcidev_t device, pci_msix_t* msix) {
#if DEBUG_LEVEL_TRACE
    DEBUG_ASSERT(pci_read(device, msix->msix_cap, 1) == PCI_MSIX_CAPID);
#endif

    uint16_t pci_msgctl = pci_read(device, msix->msix_cap + PCI_MSIX_HDR_MSGCTL, sizeof(uint16_t));
    pci_msgctl &= ~PCI_MSIX_MSGCTL_ENABLE;
    pci_msgctl |= PCI_MSIX_MSGCTL_MASK;
    pci_write(device, msix->msix_cap + PCI_MSIX_HDR_MSGCTL, sizeof(uint16_t), pci_msgctl);
}

bool pci_msix_is_enabled(pcidev_t device, pci_msix_t* msix) {
#if DEBUG_LEVEL_TRACE
    DEBUG_ASSERT(pci_read(device, msix->msix_cap, 1) == PCI_MSIX_CAPID);
#endif

    return !!(pci_read(device, msix->msix_cap + PCI_MSIX_HDR_MSGCTL, sizeof(uint16_t)) & PCI_MSIX_MSGCTL_ENABLE);
}


void pci_msix_mask(pcidev_t device, pci_msix_t* msix, uint32_t index) {
    atomic_thread_fence(memory_order_acquire);
    msix->msix_rows[index].pr_ctl |= PCI_MSIX_INTR_MASK;
    atomic_thread_fence(memory_order_release);
}

void pci_msix_unmask(pcidev_t device, pci_msix_t* msix, uint32_t index) {
    atomic_thread_fence(memory_order_acquire);
    msix->msix_rows[index].pr_ctl &= ~PCI_MSIX_INTR_MASK;
    atomic_thread_fence(memory_order_release);
}

int pci_msix_map_irq(pcidev_t device, pci_msix_t* msix, pci_irq_handler_t handler, pci_irq_data_t data, uint16_t vector) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(handler);
    DEBUG_ASSERT(msix);
    DEBUG_ASSERT(msix->msix_rows);
    DEBUG_ASSERT(msix->msix_pci.pci_msgctl_table_size >= vector);


    uint16_t index = pci_dev_register(device, handler, data, vector);
    if (index == PCI_NONE) {
#if DEBUG_LEVEL_FATAL
        kprintf("pci-msix: ERROR! No more device slots available for device %d [handler(%p), data(%p)]\n", device, handler, data);
#endif
        return errno = ENOSPC, -1;
    }

    size_t i = 0;
    for (i = 0; i < msix->msix_pci.pci_msgctl_table_size + 1; i++) {
        
        if (msix->msix_rows[i].pr_address != 0)
            continue;

        msix->msix_rows[i].pr_address = cpu_to_le64(0xFEE00000 | (current_cpu->id << 12));
        msix->msix_rows[i].pr_data    = cpu_to_le32(index + PCI_MSIX_INTR_BASE + 0x20);
        msix->msix_rows[i].pr_ctl     = cpu_to_le32(le32_to_cpu(msix->msix_rows[i].pr_ctl) | PCI_MSIX_INTR_MASK);

    }

    if (i == msix->msix_pci.pci_msgctl_table_size + 1) {
#if DEBUG_LEVEL_FATAL
        kprintf("pci-msix: ERROR! No more MSI-X table slots available for device %d [handler(%p), data(%p)]\n", device, handler, data);
#endif
        pci_dev_unregister(device);
        return errno = ENOSPC, -1;
    }
            
    arch_intr_map_irq(index + PCI_MSIX_INTR_BASE, pci_msix_interrupt_handler, ARCH_INTR_TYPE_MSI);

#if DEBUG_LEVEL_TRACE
    kprintf("pci-msix: slot %d mapped for device %d [vector(%p), handler(%p), data(%p)]\n", index, device, vector, handler, data);
#endif

    return 0;
}


int pci_msix_unmap_irq(pcidev_t device, pci_msix_t* msix) {

    uint16_t index = pci_dev_unregister(device);
    if (index == PCI_NONE) {
#if DEBUG_LEVEL_FATAL
        kprintf("pci-msix: ERROR! No device slot found for device %d\n", device);
#endif
        return errno = ESRCH, -1;
    }

    size_t i;
    for (i = 0; i < msix->msix_pci.pci_msgctl_table_size + 1; i++) {
        
        if (msix->msix_rows[i].pr_data != cpu_to_le32(index + PCI_MSIX_INTR_BASE + 0x20))
            continue;
        
        msix->msix_rows[i].pr_address = 0;
        msix->msix_rows[i].pr_data    = 0;
        msix->msix_rows[i].pr_ctl     = cpu_to_le32(le32_to_cpu(msix->msix_rows[i].pr_ctl) | PCI_MSIX_INTR_MASK);

    }

    if (i == msix->msix_pci.pci_msgctl_table_size + 1) {
#if DEBUG_LEVEL_FATAL
        kprintf("pci-msix: WARN! No MSI-X table slot found for device %d\n", device);
#endif
    }

    return 0;
}
