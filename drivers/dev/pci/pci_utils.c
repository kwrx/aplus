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
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/errno.h>
#include <aplus/endian.h>

#include <dev/interface.h>
#include <dev/pci.h>



uintptr_t pci_bar_size(pcidev_t device, int field, size_t size) {
        
    uintptr_t p = pci_read(device, field, size);
    
    switch(size) {
        case 8:
        case 4:
            pci_write(device, field, 4, 0xFFFFFFFF);
            break;

        case 2:
            pci_write(device, field, 2, 0xFFFF);
            break;

        case 1:
            pci_write(device, field, 1, 0xFF);
            break;

        default:
            DEBUG_ASSERT(0 && "Bug: Invalid Size!");
    }

    uintptr_t s = (~(pci_read(device, field, size)) + 1);


    pci_write(device, field, size, p);

    return (s & (((1ULL << (size << 3)) - 1)));
}



uintptr_t pci_find_capabilities(pcidev_t device) {

    DEBUG_ASSERT(device);

    if((pci_read(device, PCI_STATUS, 2) & PCI_STATUS_REG_CAPABILITIES) == 0)
        return PCI_NONE;

    return pci_read(device, PCI_CAPABILITIES, 1);

}



void pci_enable_bus_mastering(pcidev_t device) {

    DEBUG_ASSERT(device);

    uint32_t cmd;
    if(!((cmd = pci_read(device, PCI_COMMAND, 4)) & PCI_COMMAND_REG_BUS_MASTERING))
        pci_write(device, PCI_COMMAND, 4, cmd | PCI_COMMAND_REG_BUS_MASTERING);

}

void pci_enable_pio(pcidev_t device) {

    DEBUG_ASSERT(device);

    uint32_t cmd;
    if(!((cmd = pci_read(device, PCI_COMMAND, 4)) & PCI_COMMAND_REG_PIO))
        pci_write(device, PCI_COMMAND, 4, cmd | PCI_COMMAND_REG_PIO);

}

void pci_enable_mmio(pcidev_t device) {

    DEBUG_ASSERT(device);

    uint32_t cmd;
    if(!((cmd = pci_read(device, PCI_COMMAND, 4)) & PCI_COMMAND_REG_MMIO))
        pci_write(device, PCI_COMMAND, 4, cmd | PCI_COMMAND_REG_MMIO);

}



void pci_memcpy(pcidev_t device, void* dest, uintptr_t offset, size_t size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(dest);
    DEBUG_ASSERT(size);

    char* p = (char*) dest;

    for(size_t i = 0; i < size; i++)
        p[i] = pci_read(device, offset + i, 1);

}

