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
#include <aplus/errno.h>

#include <dev/interface.h>
#include <dev/pci.h>

#include <arch/x86/cpu.h>




MODULE_NAME("dev/pci");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static uint16_t pci_find_type(uint32_t);
static void pci_scan_hit(pci_func_t, uint32_t, void*);
static void pci_scan_func(pci_func_t, int, int, int, int, void*);
static void pci_scan_slot(pci_func_t, int, int, int, void*);
static void pci_scan_bus(pci_func_t, int, int, void*);



void pci_write(pcidev_t device, int field, size_t size, uint32_t value) {

    outl(PCI_ADDRESS_PORT, pci_get_addr(device, field));
    outl(PCI_VALUE_PORT, value);

}


uintptr_t pci_read(pcidev_t device, int field, size_t size) {
    
    outl(PCI_ADDRESS_PORT, pci_get_addr(device, field));

    switch(size) {
        case 4:
            return inl(PCI_VALUE_PORT);
        case 2:
            return inw(PCI_VALUE_PORT + (field & 2));
        case 1:
            return inb(PCI_VALUE_PORT + (field & 3));
        default:
            DEBUG_ASSERT(0 && "Bug: Invalid Size!");
    }

    return PCI_NONE;

}


uintptr_t pci_bar_size(pcidev_t device, int field, size_t size) {
        
    uintptr_t p = pci_read(device, field, size);
    
    switch(size) {
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

    return s & (((1ULL << (size << 3)) - 1));
}


static uint16_t pci_find_type(uint32_t device) {
    
    return (
        (pci_read(device, PCI_CLASS, 1) << 8) |
        (pci_read(device, PCI_SUBCLASS, 1))
    );

}


static void pci_scan_hit(pci_func_t fn, uint32_t device, void* arg) {
    
    int v = pci_read(device, PCI_VENDOR_ID, 2);
    int d = pci_read(device, PCI_DEVICE_ID, 2);

    fn(device, v, d, arg);

}


static void pci_scan_func(pci_func_t fn, int type, int bus, int slot, int func, void* arg) {

    pcidev_t d = pci_box_device(bus, slot, func);

    if(type == -1 || type == pci_find_type(d))
        pci_scan_hit(fn, d, arg);

    if(pci_find_type(d) == PCI_TYPE_BRIDGE)
        pci_scan_bus(fn, type, pci_read(d, PCI_SECONDARY_BUS, 1), arg);

}


static void pci_scan_slot(pci_func_t fn, int type, int bus, int slot, void* arg) {

    pcidev_t d = pci_box_device(bus, slot, 0);

    if(pci_read(d, PCI_VENDOR_ID, 2) == PCI_NONE)
        return;


    pci_scan_func(fn, type, bus, slot, 0, arg);

    if(!pci_read(d, PCI_HEADER_TYPE, 1))
        return;

    
    int i;
    for(i = 1; i < 8; i++) {

        pcidev_t d = pci_box_device(bus, slot, i);

        if(pci_read(d, PCI_VENDOR_ID, 2) != PCI_NONE)
            pci_scan_func(fn, type, bus, slot, i, arg);

    }

}


static void pci_scan_bus(pci_func_t fn, int type, int bus, void* arg) {

    int i;
    for(i = 0; i < 32; i++)
        pci_scan_slot(fn, type, bus, i, arg);

}


void pci_scan(pci_func_t fn, int type, void* arg) {
    
    pci_scan_bus(fn, type, 0, arg);

    if(!pci_read(0, PCI_HEADER_TYPE, 1))
        return;

    int i;
    for(i = 1; i < 8; i++) {
        
        uint32_t d = pci_box_device(0, 0, i);

        if(pci_read(d, PCI_VENDOR_ID, 2) != PCI_NONE)
            pci_scan_bus(fn, type, i, arg);
        else
            break;

    }

}



void init(const char* args) {
    (void) args;
}


void dnit(void) {

}