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
#include <aplus/errno.h>
#include <aplus/fb.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/smp.h>

#include <dev/char.h>
#include <dev/interface.h>
#include <dev/pci.h>

#include <dev/virtio/virtio-console.h>
#include <dev/virtio/virtio.h>


MODULE_NAME("virtio/virtio-console");
MODULE_DEPS("dev/interface,dev/pci,virtio/virtio-pci,virtio/virtio-queue");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static void virtconsole_init(device_t*);
static void virtconsole_dnit(device_t*);
static void virtconsole_reset(device_t*);
static ssize_t virtconsole_write(device_t*, const void*, size_t);
static ssize_t virtconsole_read(device_t*, void*, size_t);


device_t device = {

    .type = DEVICE_TYPE_CHAR,

    .name        = "hvc0",
    .description = "VIRTIO Console Device",

    .major = 229,
    .minor = 0,

    .status = DEVICE_STATUS_UNKNOWN,

    .init  = virtconsole_init,
    .dnit  = virtconsole_dnit,
    .reset = virtconsole_reset,

    .chr.io = CHAR_IO_NBF,
    .chr.write = virtconsole_write,
    .chr.read  = virtconsole_read,

};


static int negotiate_features(struct virtio_driver* driver, uint32_t* features, size_t index) {
    if (index == 0) {
        *features &= ~VIRTIO_CONSOLE_F_SIZE;
        *features &= ~VIRTIO_CONSOLE_F_MULTIPORT;
        *features &= ~VIRTIO_CONSOLE_F_EMERG_WRITE;
    }
    return 0;
}

static int setup_config(struct virtio_driver* driver, uintptr_t device_config) {
    return 0;
}

static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {
    
    device_t* config = (device_t*)arg;

    if (config->userdata != NULL)
        return;

    if (vid != VIRTIO_PCI_VENDOR)
        return;

    if (did != VIRTIO_PCI_DEVICE(VIRTIO_DEVICE_TYPE_CONSOLE))
        return;


    struct virtio_driver* driver = kcalloc(1, sizeof(struct virtio_driver), GFP_KERNEL);

    driver->type             = VIRTIO_DEVICE_TYPE_CONSOLE;
    driver->device           = device;
    driver->send_window_size = 4096;
    driver->recv_window_size = 4096;
    driver->max_queues       = 2;

    driver->negotiate = &negotiate_features;
    driver->setup     = &setup_config;
    driver->interrupt = NULL;


    if (virtio_pci_init(driver) < 0) {
#if DEBUG_LEVEL_ERROR
        kprintf("virtio-console: device %d (%X:%X) initialization failed\n", device, vid, did);
#endif
        return;
    }

    config->userdata = driver;

    virtq_send(driver, VIRTIO_CONSOLE_PORT_TX(0), "Hello World!", 13);
}


static void virtconsole_init(device_t* device) {
    DEBUG_ASSERT(device);
    virtconsole_reset(device);
}

static void virtconsole_dnit(device_t* device) {
    DEBUG_ASSERT(device);
}

static void virtconsole_reset(device_t* device) {
    DEBUG_ASSERT(device);
}

static ssize_t virtconsole_write(device_t* device, const void* buf, size_t size) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(buf);

    if (unlikely(size == 0))
        return 0;

    return virtq_send((struct virtio_driver*)device->userdata, VIRTIO_CONSOLE_PORT_TX(0), buf, size);
}

static ssize_t virtconsole_read(device_t* device, void* buf, size_t size) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(buf);

    errno = ENOSYS;
    return -1;
}

void init(const char* args) {

    if (strstr(core->boot.cmdline, "virtio=off"))
        return;

    pci_scan(&pci_find, PCI_TYPE_ALL, &device);

    if (device.userdata == NULL)
        return;

    device_mkdev(&device, 0644);
}

void dnit(void) {
}
