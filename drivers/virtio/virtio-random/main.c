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

#include <dev/virtio/virtio-random.h>
#include <dev/virtio/virtio.h>


MODULE_NAME("virtio/virtio-random");
MODULE_DEPS("dev/interface,dev/pci,virtio/virtio-pci,virtio/virtio-queue");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static void virtrandom_init(device_t*);
static void virtrandom_dnit(device_t*);
static void virtrandom_reset(device_t*);
static ssize_t virtrandom_write(device_t*, const void*, size_t);
static ssize_t virtrandom_read(device_t*, void*, size_t);


device_t device = {

    .type = DEVICE_TYPE_CHAR,

    .name        = "hwrng",
    .description = "VIRTIO Random Device",

    .major = 10,
    .minor = 183,

    .status = DEVICE_STATUS_UNKNOWN,

    .init  = virtrandom_init,
    .dnit  = virtrandom_dnit,
    .reset = virtrandom_reset,

    .chr.io = CHAR_IO_NBF,
    .chr.write = virtrandom_write,
    .chr.read  = virtrandom_read,

};


static int negotiate_features(struct virtio_driver* driver, uint32_t* features, size_t index) {
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

    if (did != VIRTIO_PCI_DEVICE(VIRTIO_DEVICE_TYPE_ENTROPY_SOURCE))
        return;


    struct virtio_driver* driver = kcalloc(1, sizeof(struct virtio_driver), GFP_KERNEL);

    driver->type             = VIRTIO_DEVICE_TYPE_ENTROPY_SOURCE;
    driver->device           = device;
    driver->send_window_size = 4096;
    driver->recv_window_size = 8192;
    driver->max_queues       = 0;

    driver->negotiate = &negotiate_features;
    driver->setup     = &setup_config;
    driver->interrupt = NULL;


    if (virtio_pci_init(driver) < 0) {
#if DEBUG_LEVEL_ERROR
        kprintf("virtio-random: device %d (%X:%X) initialization failed\n", device, vid, did);
#endif
        return;
    }

    config->userdata = driver;
}


static void virtrandom_init(device_t* device) {
    DEBUG_ASSERT(device);
    virtrandom_reset(device);
}

static void virtrandom_dnit(device_t* device) {
    DEBUG_ASSERT(device);
}

static void virtrandom_reset(device_t* device) {
    DEBUG_ASSERT(device);
}

static ssize_t virtrandom_write(device_t* device, const void* buf, size_t size) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(buf);

    errno = ENOSPC;
    return -1;
}

static ssize_t virtrandom_read(device_t* device, void* buf, size_t size) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(buf);

    if (unlikely(size == 0))
        return 0;

    return virtq_recv((struct virtio_driver*)device->userdata, VIRTIO_RANDOM_QUEUE_DATA, buf, size);
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
