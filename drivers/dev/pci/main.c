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
#include <aplus/module.h>
#include <aplus/vfs.h>

#include <dev/interface.h>
#include <dev/pci.h>



MODULE_NAME("dev/pci");
MODULE_DEPS("arch/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


void init(const char* args) {

#if DEBUG_LEVEL_TRACE

    for (int i = 0; i < 256; i++) {

        uint16_t v = pci_read(i, PCI_VENDOR_ID, 2);

        if (v == 0xFFFF)
            continue;

        uint16_t d  = pci_read(i, PCI_DEVICE_ID, 2);
        uint16_t ss = pci_read(i, PCI_SUBSYSID, 2);
        uint16_t sv = pci_read(i, PCI_SUBVENID, 2);

        kprintf("PCI: %x:%x.%x: %x:%x (sub: %x:%x) [%x:%x] (%x:%x) (%x:%x) (%x:%x) (%x:%x)\n", pci_extract_bus(i), pci_extract_slot(i), pci_extract_func(i), v, d, sv, ss, pci_read(i, PCI_CLASS, 1), pci_read(i, PCI_SUBCLASS, 1),
                pci_read(i, PCI_PROG_IF, 1), pci_read(i, PCI_REVISION_ID, 1), pci_read(i, PCI_BIST, 1), pci_read(i, PCI_HEADER_TYPE, 1), pci_read(i, PCI_LATENCY_TIMER, 1), pci_read(i, PCI_CACHE_LINE_SIZE, 1),
                pci_read(i, PCI_INTERRUPT_LINE, 1), pci_read(i, PCI_INTERRUPT_PIN, 1));


        uintptr_t bar0 = pci_read(i, PCI_BAR0, 4);
        uintptr_t bar1 = pci_read(i, PCI_BAR1, 4);
        uintptr_t bar2 = pci_read(i, PCI_BAR2, 4);
        uintptr_t bar3 = pci_read(i, PCI_BAR3, 4);
        uintptr_t bar4 = pci_read(i, PCI_BAR4, 4);
        uintptr_t bar5 = pci_read(i, PCI_BAR5, 4);

        if (bar0)
            kprintf("     BAR0: %x (%x)\n", bar0, pci_bar_size(i, PCI_BAR0, 4));

        if (bar1)
            kprintf("     BAR1: %x (%x)\n", bar1, pci_bar_size(i, PCI_BAR1, 4));

        if (bar2)
            kprintf("     BAR2: %x (%x)\n", bar2, pci_bar_size(i, PCI_BAR2, 4));

        if (bar3)
            kprintf("     BAR3: %x (%x)\n", bar3, pci_bar_size(i, PCI_BAR3, 4));

        if (bar4)
            kprintf("     BAR4: %x (%x)\n", bar4, pci_bar_size(i, PCI_BAR4, 4));

        if (bar5)
            kprintf("     BAR5: %x (%x)\n", bar5, pci_bar_size(i, PCI_BAR5, 4));
    }

#endif

    (void)args;
}


void dnit(void) {
}
