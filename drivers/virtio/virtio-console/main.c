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
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/hal.h>
#include <aplus/fb.h>
#include <aplus/errno.h>

#include <dev/interface.h>
#include <dev/char.h>
#include <dev/pci.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtio-console.h>


MODULE_NAME("virtio/virtio-console");
MODULE_DEPS("dev/interface,dev/pci,virtio/virtio-pci,virtio/virtio-queue");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");





static void virtconsole_init(device_t*);
static void virtconsole_dnit(device_t*);
static void virtconsole_reset(device_t*);


device_t device = {

    .type = DEVICE_TYPE_CHAR,

    .name = "vport0p1",
    .description = "VIRTIO Console Device",

    .major = 10, // FIXME: change values
    .minor = 243,

    .status = DEVICE_STATUS_UNKNOWN,

    .init = virtconsole_init,
    .dnit = virtconsole_dnit,
    .reset = virtconsole_reset,

    .chr.io =    CHAR_IO_NBF,
    //.chr.write = &virtconsole_write,
    //.chr.read =  &virtconsole_read,

};





static void virtconsole_init(device_t* device) {

    DEBUG_ASSERT(device);    
    DEBUG_ASSERT(device->mmiobase);    

    
    virtconsole_reset(device);

}


static void virtconsole_dnit(device_t* device) {

    DEBUG_ASSERT(device);    
    DEBUG_ASSERT(device->mmiobase);    

}


static void virtconsole_reset(device_t* device) {

    DEBUG_ASSERT(device);


}

static int negotiate_features(struct virtio_driver* driver, uint32_t* features, size_t index) {
    
    if(index == 0) {

        *features &= ~VIRTIO_CONSOLE_F_SIZE;
        *features &= ~VIRTIO_CONSOLE_F_MULTIPORT;
        *features &= ~VIRTIO_CONSOLE_F_EMERG_WRITE;

    }

    return 0;

}

static int setup_config(struct virtio_driver* driver, uintptr_t device_config) {
    return 0;
}

static int interrupt_handler(pcidev_t device, irq_t vector, struct virtio_driver* driver) {
    return kprintf("!!!!!!!!!!!!!!!RECEIVED MSI-X INTERRUPT!!!!!!!!!!!!!!!!!!\n"), 0;
}


static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {

    if(vid != VIRTIO_PCI_VENDOR)
        return;

    if(did != VIRTIO_PCI_DEVICE(VIRTIO_DEVICE_TYPE_CONSOLE))
        return;


    struct virtio_driver driver = { 0 };

    driver.type = VIRTIO_DEVICE_TYPE_CONSOLE;
    driver.device = device;
    driver.send_window_size = 4096;
    driver.recv_window_size = 4096;

    driver.negotiate = &negotiate_features;
    driver.setup = &setup_config;
    driver.interrupt = &interrupt_handler;


    if(virtio_pci_init(&driver) < 0) {

#if DEBUG_LEVEL_ERROR
        kprintf("virtio-console: device %d (%X:%X) initialization failed\n", device, vid, did);
#endif

        return;

    }


    virtq_send(&driver, VIRTIO_CONSOLE_PORT_TX(0), "Hello World!", 13);


}


void init(const char* args) {

    if(strstr(core->boot.cmdline, "virtio=off"))
        return;

    pci_scan(&pci_find, PCI_TYPE_ALL, NULL);

}

void dnit(void) {

}