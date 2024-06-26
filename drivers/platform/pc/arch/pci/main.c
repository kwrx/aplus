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

#include <arch/x86/cpu.h>



MODULE_NAME("arch/pci");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



void pci_write(pcidev_t device, int field, size_t size, uint64_t value) {

    if (unlikely(size == 8)) {

        outl(PCI_ADDRESS_PORT, pci_get_addr(device, field));
        outl(PCI_VALUE_PORT, cpu_to_le32(value & 0xFFFFFFFF));

        outl(PCI_ADDRESS_PORT, pci_get_addr(device, field + 4));
        outl(PCI_VALUE_PORT, cpu_to_le32(value >> 32));

    } else {

        outl(PCI_ADDRESS_PORT, pci_get_addr(device, field));

        switch (size) {
            case 4:
                return outl(PCI_VALUE_PORT, cpu_to_le32(value));
            case 2:
                return outw(PCI_VALUE_PORT, cpu_to_le16(value));
            case 1:
                return outb(PCI_VALUE_PORT, value);
            default:
                PANIC_ASSERT(0 && "Bug: Invalid Size!");
        }
    }
}


uint64_t pci_read(pcidev_t device, int field, size_t size) {

    if (unlikely(size == 8)) {

        outl(PCI_ADDRESS_PORT, pci_get_addr(device, field));
        uint64_t low = inl(PCI_VALUE_PORT);

        outl(PCI_ADDRESS_PORT, pci_get_addr(device, field + 4));
        uint64_t high = inl(PCI_VALUE_PORT);

        return (le64_to_cpu(high) << 32ULL) | le64_to_cpu(low);

    } else {

        outl(PCI_ADDRESS_PORT, pci_get_addr(device, field));

        switch (size) {
            case 4:
                return le32_to_cpu(inl(PCI_VALUE_PORT));
            case 2:
                return le16_to_cpu(inw(PCI_VALUE_PORT + (field & 2)));
            case 1:
                return inb(PCI_VALUE_PORT + (field & 3));
            default:
                PANIC_ASSERT(0 && "Bug: Invalid Size!");
        }
    }

    return PCI_NONE;
}



void init(const char* args) {
    (void)args;
}


void dnit(void) {
}
