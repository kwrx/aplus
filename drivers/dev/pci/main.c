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



MODULE_NAME("dev/pci");
MODULE_DEPS("arch/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


void init(const char* args) {

#if DEBUG_LEVEL_TRACE

    for(int i = 0; i < 256; i++) {

        uint16_t v = pci_read(i, PCI_VENDOR_ID, 2);

        if(v == 0xFFFF)
            continue;

        uint16_t d  = pci_read(i, PCI_DEVICE_ID, 2);
        uint16_t ss = pci_read(i, PCI_SUBSYSID, 2);
        uint16_t sv = pci_read(i, PCI_SUBVENID, 2);

        kprintf("PCI: %x:%x.%x: %x:%x (sub: %x:%x) [%x:%x] (%x:%x) (%x:%x) (%x:%x) (%x:%x)\n", 
            pci_extract_bus(i), pci_extract_slot(i), pci_extract_func(i),
            v, d,
            sv, ss,
            pci_read(i, PCI_CLASS, 1), pci_read(i, PCI_SUBCLASS, 1),
            pci_read(i, PCI_PROG_IF, 1), pci_read(i, PCI_REVISION_ID, 1),
            pci_read(i, PCI_BIST, 1), pci_read(i, PCI_HEADER_TYPE, 1),
            pci_read(i, PCI_LATENCY_TIMER, 1), pci_read(i, PCI_CACHE_LINE_SIZE, 1),
            pci_read(i, PCI_INTERRUPT_LINE, 1), pci_read(i, PCI_INTERRUPT_PIN, 1)
        );


    }

#endif

    (void) args;
}


void dnit(void) {

}