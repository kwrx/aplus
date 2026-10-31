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
#include <stdio.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/sysfs.h>

#include <dev/pci.h>


static void pci_sysfs_name(pcidev_t device, char* buf, size_t size) {
    snprintf(buf, size, "0000:%02x:%02x.%x", pci_extract_bus(device), pci_extract_slot(device), pci_extract_func(device));
}


static void pci_sysfs_add(pcidev_t device, uint16_t vendor_id, uint16_t device_id, void* arg) {

    (void)arg;

    uint8_t class    = pci_read(device, PCI_CLASS, 1);
    uint8_t subclass = pci_read(device, PCI_SUBCLASS, 1);
    uint8_t progif   = pci_read(device, PCI_PROG_IF, 1);

    uint16_t subvendor = 0;
    uint16_t subdevice = 0;

    if ((pci_read(device, PCI_HEADER_TYPE, 1) & 0x7F) == PCI_HEADER_TYPE_DEVICE) {
        subvendor = pci_read(device, PCI_SUBSYSID, 2);
        subdevice = pci_read(device, PCI_SUBVENID, 2);
    }


    char name[16];
    pci_sysfs_name(device, name, sizeof(name));

    char uevent[256];
    snprintf(uevent, sizeof(uevent),
             "PCI_CLASS=%X\n"
             "PCI_ID=%04X:%04X\n"
             "PCI_SUBSYS_ID=%04X:%04X\n"
             "PCI_SLOT_NAME=%s\n"
             "MODALIAS=pci:v%08Xd%08Xsv%08Xsd%08Xbc%02Xsc%02Xi%02X\n",
             (class << 16) | (subclass << 8) | progif, vendor_id, device_id, subvendor, subdevice, name, vendor_id, device_id, subvendor, subdevice, class, subclass, progif);

    sysfs_device_bus_add("pci", name, uevent);
}

static void pci_sysfs_remove(pcidev_t device, uint16_t vendor_id, uint16_t device_id, void* arg) {

    (void)vendor_id;
    (void)device_id;
    (void)arg;

    char name[16];
    pci_sysfs_name(device, name, sizeof(name));

    sysfs_device_bus_remove("pci", name);
}


void pci_sysfs_init(void) {
    pci_scan(pci_sysfs_add, PCI_TYPE_ALL, NULL);
}

void pci_sysfs_dnit(void) {
    pci_scan(pci_sysfs_remove, PCI_TYPE_ALL, NULL);
}
