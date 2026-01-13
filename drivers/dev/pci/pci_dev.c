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

static struct {
    uint16_t vector;
    pcidev_t device;
    pci_irq_handler_t handler;
    pci_irq_data_t data;
} pci_devices[PCI_DEVICES_MAX] = {0};

static spinlock_t pci_dev_lock = SPINLOCK_INIT_WITH_FLAGS(SPINLOCK_FLAGS_CPU_OWNER);

uint16_t pci_dev_register(pcidev_t device, pci_irq_handler_t handler, pci_irq_data_t data, uint16_t vector) {

    scoped_lock(&pci_dev_lock) {

        for (size_t i = 0; i < PCI_DEVICES_MAX; i++) {

            if (pci_devices[i].device)
                continue;

            pci_devices[i].vector  = vector;
            pci_devices[i].device  = device;
            pci_devices[i].handler = handler;
            pci_devices[i].data    = data;

            return i;

        }
    }
    return PCI_NONE;
}

uint16_t pci_dev_unregister(pcidev_t device) {

    scoped_lock(&pci_dev_lock) {

        for (size_t i = 0; i < PCI_DEVICES_MAX; i++) {

            if (pci_devices[i].device != device)
                continue;

            pci_devices[i].device  = 0;
            pci_devices[i].vector  = 0;
            pci_devices[i].handler = NULL;
            pci_devices[i].data    = NULL;

            return i;
        }
    }
    return PCI_NONE;
}

pcidev_t pci_dev_get_device(uint16_t index) {
    DEBUG_ASSERT(index < PCI_DEVICES_MAX);
    scoped_lock(&pci_dev_lock) {
        return pci_devices[index].device;
    }
    return 0;
}

int pci_dev_call_handler(uint16_t index, irq_t irq) {
    DEBUG_ASSERT(index < PCI_DEVICES_MAX);
    scoped_lock(&pci_dev_lock) {
        if (pci_devices[index].handler) {
            pci_devices[index].handler(pci_devices[index].device, irq, pci_devices[index].data, pci_devices[index].vector);
        } else {
            return errno = ESRCH, -1;
        }
    }
    return 0;
}
