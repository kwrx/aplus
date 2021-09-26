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

#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/hal.h>
#include <aplus/errno.h>
#include <aplus/endian.h>

#include <dev/interface.h>
#include <dev/pci.h>


#define PCI_INTX_DEVICES_MAX                128


static struct {
    pcidev_t device;
    pci_irq_handler_t handler;
    pci_irq_data_t data;
} pci_intx_devices[PCI_INTX_DEVICES_MAX] = { 0 };


static void pci_intx_interrupt_handler(void* frame, irq_t irq) {

    size_t i;
    for(i = 0; i < PCI_INTX_DEVICES_MAX; i++) {

        if(!pci_intx_devices[i].device)
            continue;

        if(!pci_intx_devices[i].handler)
            continue;

        if((pci_read(pci_intx_devices[i].device, PCI_INTERRUPT_LINE, 1) != irq))
            continue;

        if((pci_read(pci_intx_devices[i].device, PCI_STATUS, 2) & PCI_STATUS_REG_INTERRUPT) == 0)
            continue;

        if((pci_read(pci_intx_devices[i].device, PCI_COMMAND, 2) & PCI_COMMAND_REG_INTR_DISABLE) != 0)
            continue;


        pci_intx_devices[i].handler(pci_intx_devices[i].device, irq, pci_intx_devices[i].data);

    }

}


int pci_intx_map_irq(irq_t irq, pcidev_t device, pci_irq_handler_t handler, pci_irq_data_t data) {

    size_t i;
    for(i = 0; i < PCI_INTX_DEVICES_MAX; i++) {

        if(pci_intx_devices[i].device)
            continue;


        pci_intx_devices[i].device = device;
        pci_intx_devices[i].handler = handler;
        pci_intx_devices[i].data = data;

        arch_intr_map_irq(irq, pci_intx_interrupt_handler);


#if defined(DEBUG) && DEBUG_LEVEL >= 4
        kprintf("pci-intx: slot %d mapped for device %d [irq(%p), handler(%p), data(%p)]\n", i, device, irq, handler, data);
#endif

        return 0;

    }
    

#if defined(DEBUG) && DEBUG_LEVEL >= 0
    kprintf("pci-intx: ERROR! No more device slots available for device %d [irq(%p), handler(%p), data(%p)]\n", device, irq, handler, data);
#endif

    return -ENOSPC;

}


int pci_intx_unmap_irq(pcidev_t device) {

    size_t i;
    for(i = 0; i < PCI_INTX_DEVICES_MAX; i++) {

        if(pci_intx_devices[i].device != device)
            continue;


        pci_intx_devices[i].device = 0;
        pci_intx_devices[i].handler = NULL;
        pci_intx_devices[i].data = NULL;

        return 0;

    }


#if defined(DEBUG) && DEBUG_LEVEL >= 0
    kprintf("pci-intx: ERROR! No device slot found for device %d\n", device);
#endif

    return -ESRCH;

}


void pci_intx_mask(pcidev_t device) {
    pci_write(device, PCI_COMMAND, 2, pci_read(device, PCI_COMMAND, 2) | PCI_COMMAND_REG_INTR_DISABLE);
}

void pci_intx_unmask(pcidev_t device) {
    pci_write(device, PCI_COMMAND, 2, pci_read(device, PCI_COMMAND, 2) & ~PCI_COMMAND_REG_INTR_DISABLE);
}



