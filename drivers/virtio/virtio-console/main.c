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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/fb.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
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


/**
 * @brief How much of the receive queue to stock, capped by what the queue has.
 */

#define VIRTCONSOLE_BUFFERS 8


struct virtconsole {

    struct virtio_driver* driver;

    /* Serializes readers against each other: a read consumes part of a buffer and leaves
       the rest for the next one, which two readers at once would tear. */
    spinlock_t lock;

    /* A receive buffer the device has filled that the last read did not finish with.
       VIRTQ_DESC_NONE when there is none. */
    struct {
        uint16_t desc;
        uint32_t offset;
        uint32_t length;
    } pending;
};


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


/**
 * @brief Stocks the receive queue, which the device drops input into and cannot read from while empty.
 *
 * @param vc The port to stock.
 * @return The number of buffers handed to the device.
 */

static size_t virtconsole_fill(struct virtconsole* vc) {

    DEBUG_ASSERT(vc);

    const uint16_t q = VIRTIO_CONSOLE_PORT_RX(0);

    size_t posted = 0;

    for (size_t i = 0; i < VIRTCONSOLE_BUFFERS; i++) {

        uint16_t desc = virtq_alloc_descriptor(vc->driver, q, VIRTQ_REQUEST_POSTED);

        if (desc == VIRTQ_DESC_NONE)
            break;

        virtq_provide(vc->driver, q, desc, vc->driver->recv_window_size);

        posted++;
    }

    if (posted)
        virtq_notify(vc->driver, q);

#if DEBUG_LEVEL_TRACE
    kprintf("virtio-console: device %d stocked the receive queue with %d buffers\n", vc->driver->device, posted);
#endif

    return posted;
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

    if (unlikely(!driver))
        return;

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
        kfree(driver);
        return;
    }


    struct virtconsole* vc = kcalloc(1, sizeof(struct virtconsole), GFP_KERNEL);

    if (unlikely(!vc)) {
        virtio_pci_dnit(driver);
        kfree(driver);
        return;
    }

    vc->driver       = driver;
    vc->pending.desc = VIRTQ_DESC_NONE;

    spinlock_init(&vc->lock);

    virtconsole_fill(vc);

    config->userdata = vc;
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

    struct virtio_driver* driver = ((struct virtconsole*)device->userdata)->driver;

    if (unlikely(size == 0))
        return 0;

    for (size_t sent = 0; sent < size;) {

        ssize_t ret = virtq_send(driver, VIRTIO_CONSOLE_PORT_TX(0), (const uint8_t*)buf + sent, MIN(size - sent, driver->send_window_size));

        if (unlikely(ret < 0))
            return sent ? (ssize_t)sent : ret;

        sent += (size_t)ret;
    }

    return (ssize_t)size;
}


/**
 * @brief Drains what the device has put in the receive queue, holding a partly read buffer for the next call.
 *
 * @param device The port to read from.
 * @param buf The buffer to fill.
 * @param size The number of bytes asked for.
 * @return The number of bytes read, or a negative errno.
 */

static ssize_t virtconsole_read(device_t* device, void* buf, size_t size) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(buf);

    if (unlikely(size == 0))
        return 0;


    struct virtconsole* vc       = (struct virtconsole*)device->userdata;
    struct virtio_driver* driver = vc->driver;

    const uint16_t q = VIRTIO_CONSOLE_PORT_RX(0);

    size_t done     = 0;
    bool recycled   = false;

    scoped_lock(&vc->lock) {

        while (done < size) {

            if (vc->pending.desc == VIRTQ_DESC_NONE) {

                uint16_t desc;
                uint32_t length;

                if (!virtq_reap(driver, q, &desc, &length))
                    break;

                if (desc == VIRTQ_DESC_NONE)
                    continue;

                if (unlikely(!length)) {

                    virtq_provide(driver, q, desc, driver->recv_window_size);
                    recycled = true;

                    continue;
                }

                vc->pending.desc   = desc;
                vc->pending.offset = 0;
                vc->pending.length = MIN(length, (uint32_t)driver->recv_window_size);
            }


            size_t chunk = MIN(size - done, (size_t)(vc->pending.length - vc->pending.offset));

            memcpy((uint8_t*)buf + done, (const void*)(virtq_recvbuf(driver, q, vc->pending.desc) + vc->pending.offset), chunk);

            done += chunk;
            vc->pending.offset += chunk;

            if (vc->pending.offset == vc->pending.length) {

                virtq_provide(driver, q, vc->pending.desc, driver->recv_window_size);
                recycled = true;

                vc->pending.desc = VIRTQ_DESC_NONE;
            }
        }
    }

    if (recycled)
        virtq_notify(driver, q);

    return (ssize_t)done;
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

    if (device.userdata == NULL)
        return;

    device_unlink(&device);
}
