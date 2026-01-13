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


static void pci_msi_interrupt_handler(void* frame, irq_t irq) {

    DEBUG_ASSERT(irq > PCI_MSI_INTR_BASE - 1);
    DEBUG_ASSERT(irq < PCI_MSI_INTR_BASE + PCI_DEVICES_MAX);

    uint16_t index = irq - PCI_MSI_INTR_BASE;

    pcidev_t device = pci_dev_get_device(index);
    if (device == 0)
        return;

    pci_dev_call_handler(index, irq);
}

static bool pci_msi_is64bit(pcidev_t device, uintptr_t msi_cap) {
    return !!(pci_read(device, msi_cap + PCI_MSI_HDR_MSGCTL, sizeof(uint16_t)) & PCI_MSI_MSGCTL_ADDR_64BIT);
}

int pci_find_msi(pcidev_t device, pci_msi_t* mptr) {

    uintptr_t caps;
    if ((caps = pci_find_capabilities(device)) == PCI_NONE)
        return PCI_NONE;

    if (mptr)
        mptr->msi_cap = caps;

    uint8_t cap_id = 0;
    uint8_t cap_next = 0;
    do {

        if ((cap_id = pci_read(device, caps + PCI_MSI_HDR_CAPID, sizeof(uint8_t))) != PCI_MSI_CAPID)
            return PCI_NONE;

        cap_next = pci_read(device, caps + PCI_MSI_HDR_CAPNEXT, sizeof(uint8_t));
        

        uint16_t msgctl = pci_read(device, caps + PCI_MSI_HDR_MSGCTL, sizeof(uint16_t));
        msgctl &= ~PCI_MSI_MSGCTL_ENABLE;
        msgctl &= ~PCI_MSI_MSGCTL_MSG_COUNT_MASK;
        pci_write(device, caps + PCI_MSI_HDR_MSGCTL, sizeof(uint16_t), msgctl);

    } while((caps = cap_next) != 0);

    return 0;
}



void pci_msi_enable(pcidev_t device, pci_msi_t* msi) {
#if DEBUG_LEVEL_TRACE
    DEBUG_ASSERT(pci_read(device, msi->msi_cap + PCI_MSI_HDR_CAPID, sizeof(uint8_t)) == PCI_MSI_CAPID);
#endif

    uint16_t msgctl = pci_read(device, msi->msi_cap + PCI_MSI_HDR_MSGCTL, sizeof(uint16_t));
    msgctl |= PCI_MSI_MSGCTL_ENABLE;
    pci_write(device, msi->msi_cap + PCI_MSI_HDR_MSGCTL, sizeof(uint16_t), msgctl);
}

void pci_msi_disable(pcidev_t device, pci_msi_t* msi) {
#if DEBUG_LEVEL_TRACE
    DEBUG_ASSERT(pci_read(device, msi->msi_cap + PCI_MSI_HDR_CAPID, sizeof(uint8_t)) == PCI_MSI_CAPID);
#endif

    uint16_t msgctl = pci_read(device, msi->msi_cap + PCI_MSI_HDR_MSGCTL, sizeof(uint16_t));
    msgctl &= ~PCI_MSI_MSGCTL_ENABLE;
    pci_write(device, msi->msi_cap + PCI_MSI_HDR_MSGCTL, sizeof(uint16_t), msgctl);
}

bool pci_msi_is_enabled(pcidev_t device, pci_msi_t* msi) {
#if DEBUG_LEVEL_TRACE
    DEBUG_ASSERT(pci_read(device, msi->msi_cap + PCI_MSI_HDR_CAPID, sizeof(uint8_t)) == PCI_MSI_CAPID);
#endif

    return (pci_read(device, msi->msi_cap + PCI_MSI_HDR_MSGCTL, sizeof(uint16_t)) & PCI_MSI_MSGCTL_ENABLE) != 0;
}

int pci_msi_map_irq(pcidev_t device, pci_msi_t* msi, pci_irq_handler_t handler, pci_irq_data_t data) {

    uint16_t index = pci_dev_register(device, handler, data, 0);
    if (index == PCI_NONE) {
#if DEBUG_LEVEL_FATAL
        kprintf("pci-msi: ERROR! No more device slots available for device %d [handler(%p), data(%p)]\n", device, handler, data);
#endif
        return errno = ENOSPC, -1;
    }

    arch_intr_map_irq(index + PCI_MSI_INTR_BASE, pci_msi_interrupt_handler, ARCH_INTR_TYPE_MSI);

    if (pci_msi_is64bit(device, msi->msi_cap)) {
        pci_write(device, msi->msi_cap + PCI_MSI_HDR_MSGADDR, sizeof(uint64_t), 0xFEE00000 | ((uint64_t)(current_cpu->id) << 12));
        pci_write(device, msi->msi_cap + PCI_MSI_HDR_MSGADDR + sizeof(uint64_t), sizeof(uint16_t), index + PCI_MSI_INTR_BASE + 0x20);
    } else {
        pci_write(device, msi->msi_cap + PCI_MSI_HDR_MSGADDR, sizeof(uint32_t), 0xFEE00000 | (current_cpu->id << 12));
        pci_write(device, msi->msi_cap + PCI_MSI_HDR_MSGADDR + sizeof(uint32_t), sizeof(uint16_t), index + PCI_MSI_INTR_BASE + 0x20);
    }

    #if DEBUG_LEVEL_TRACE
    kprintf("pci-msi: slot %d mapped for device %d [handler(%p), data(%p)]\n", index, device, handler, data);
    #endif

    atomic_thread_fence(memory_order_seq_cst);
    return 0;
}


int pci_msi_unmap_irq(pcidev_t device, pci_msi_t* msi) {

    uint16_t index = pci_dev_unregister(device);
    if (index == PCI_NONE) {
#if DEBUG_LEVEL_FATAL
        kprintf("pci-msi: ERROR! No device slot found for device %d\n", device);
#endif
        return errno = ESRCH, -1;
    }

    return 0;
}
