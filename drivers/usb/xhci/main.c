/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <stdint.h>
#include <string.h>

#include <dev/pci.h>
#include <dev/usb/usb.h>

#include "xhci.h"


MODULE_NAME("usb/xhci");
MODULE_DEPS("dev/pci,usb/core");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#define XHCI_MAX_CONTROLLERS 8
#define XHCI_PROG_IF         0x30


/** @brief An xHCI controller found on PCI, with its register BAR mapped. */
struct xhci_pci {
    pcidev_t device;
    uintptr_t mmio;
};


static struct xhci_pci controllers[XHCI_MAX_CONTROLLERS];
static size_t num_controllers = 0;



/**
 * @brief Brings a controller up on the USB thread.
 * @param arg The controller's struct xhci_pci.
 */
static void xhci_start(void* arg) {

    struct xhci_pci* xp = arg;

    if (!xhci_pci_init(xp->device, xp->mmio))
        kprintf("xhci: ERROR! controller at %d failed to initialize\n", xp->device);
}


/**
 * @brief Claims every xHCI controller pci_scan() reports.
 * @param device The PCI address.
 * @param vid The vendor id.
 * @param did The device id.
 * @param arg Unused.
 */
static void xhci_pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {

    if (num_controllers >= XHCI_MAX_CONTROLLERS)
        return;

    if (pci_read(device, PCI_PROG_IF, 1) != XHCI_PROG_IF)
        return;

    pci_enable_mmio(device);
    pci_enable_bus_mastering(device);

    uintptr_t mmio = pci_map_bar(device, PCI_BAR0, NULL);

    if (unlikely(!mmio)) {
        kprintf("xhci: ERROR! %04X:%04X at %d has no usable register BAR\n", vid, did, device);
        return;
    }

#if DEBUG_LEVEL_INFO
    kprintf("xhci: %04X:%04X at %d [mmio(%p)]\n", vid, did, device, mmio);
#endif

    struct xhci_pci* xp = &controllers[num_controllers++];

    xp->device = device;
    xp->mmio   = mmio;

    if (usb_add_controller(xhci_start, xp) < 0)
        kprintf("xhci: ERROR! no room for controller at %d\n", device);
}



void init(const char* args) {

    if (strstr(core->boot.cmdline, "usb=off"))
        return;

    pci_scan(&xhci_pci_find, PCI_TYPE_USB, NULL);
}


void dnit(void) {
}
