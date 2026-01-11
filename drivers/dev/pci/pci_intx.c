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


static void pci_intx_interrupt_handler(void* frame, irq_t irq) {

    for (size_t index = 0; index < PCI_DEVICES_MAX; index++) {

        pcidev_t device = pci_dev_get_device(index);
        if (device == 0)
            continue;

        if ((pci_read(device, PCI_INTERRUPT_LINE, 1) != irq))
            continue;

        if ((pci_read(device, PCI_STATUS, 2) & PCI_STATUS_REG_INTERRUPT) == 0)
            continue;

        if ((pci_read(device, PCI_COMMAND, 2) & PCI_COMMAND_REG_INTR_DISABLE) != 0)
            continue;

        pci_dev_call_handler(index, irq);

    }
}


int pci_intx_map_irq(pcidev_t device, irq_t irq, pci_irq_handler_t handler, pci_irq_data_t data) {

    uint16_t index = pci_dev_register(device, handler, data, 0);
    if (index == PCI_NONE) {
#if DEBUG_LEVEL_FATAL
        kprintf("pci-intx: ERROR! No more device slots available for device %d [irq(%p), handler(%p), data(%p)]\n", device, irq, handler, data);
#endif
        return errno = ENOSPC, -1;
    }

    arch_intr_map_irq(irq, pci_intx_interrupt_handler, ARCH_INTR_TYPE_PCI);
    return 0;
}


int pci_intx_unmap_irq(pcidev_t device) {

    uint16_t index = pci_dev_unregister(device);
    if (index == PCI_NONE) {
#if DEBUG_LEVEL_FATAL
        kprintf("pci-intx: ERROR! Device %d not found during unmap\n", device);
#endif
        return errno = ESRCH, -1;
    }

    return 0;
}


void pci_intx_mask(pcidev_t device) {
    pci_write(device, PCI_COMMAND, 2, pci_read(device, PCI_COMMAND, 2) | PCI_COMMAND_REG_INTR_DISABLE);
}

void pci_intx_unmask(pcidev_t device) {
    pci_write(device, PCI_COMMAND, 2, pci_read(device, PCI_COMMAND, 2) & ~PCI_COMMAND_REG_INTR_DISABLE);
}
