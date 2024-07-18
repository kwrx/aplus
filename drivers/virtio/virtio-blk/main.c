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

#include <dev/block.h>
#include <dev/interface.h>
#include <dev/pci.h>

#include <dev/virtio/virtio-blk.h>
#include <dev/virtio/virtio.h>


MODULE_NAME("virtio/virtio-blk");
MODULE_DEPS("dev/interface,dev/pci,dev/block,virtio/virtio-pci,virtio/virtio-queue");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static void virtblk_init(device_t* device);
static void virtblk_dnit(device_t* device);
static void virtblk_reset(device_t* device);


static void virtblk_init(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    virtblk_reset(device);
}


static void virtblk_dnit(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
}


static void virtblk_reset(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    virtio_driver_t* driver     = (virtio_driver_t*)device->userdata;
    virtio_blk_config_t* config = (virtio_blk_config_t*)&driver->internals.device_config;

    device->blk.blksize  = MAX(config->blk_size, MAX(config->geometry.blk_size, 512));
    device->blk.blkcount = config->capacity;
    device->blk.blkmax   = 1; // config->seg_max;

    kprintf("virtio-blk: %s %s %d %d %d\n", device->name, device->description, device->blk.blksize, device->blk.blkcount, device->blk.blkmax);
}


static ssize_t virtblk_read(device_t* device, void* buf, off_t offset, size_t count) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    virtio_driver_t* driver = (virtio_driver_t*)device->userdata;


    for (size_t sector = 0; sector < count; sector++) {

        virtio_blk_request_t request;

        request.hdr.type   = VIRTIO_BLK_T_IN;
        request.hdr.sector = offset + sector;

        if (virtq_sendrecv(driver, 0, &request, sizeof(request.hdr), &request.data, device->blk.blksize + sizeof(uint8_t)) < 0) {
            return errno = EIO, -1;
        }


        uint8_t status = request.data[device->blk.blksize];

        switch (status) {
            case VIRTIO_BLK_S_OK:
                break;
            case VIRTIO_BLK_S_IOERR:
                return errno = EIO, -1;
            case VIRTIO_BLK_S_UNSUPP:
                return errno = ENOSYS, -1;
            default:
                kpanicf("virtio-blk: unknown status %d\n", status);
        }


        memcpy(buf, &request.data, device->blk.blksize);

        buf += device->blk.blksize;
    }

    return count;
}

static ssize_t virtblk_write(device_t* device, const void* buf, off_t offset, size_t count) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    virtio_driver_t* driver = (virtio_driver_t*)device->userdata;


    for (size_t sector = 0; sector < count; sector++) {

        virtio_blk_request_t request;

        request.hdr.type   = VIRTIO_BLK_T_OUT;
        request.hdr.sector = offset + sector;

        memcpy(&request.data, buf, device->blk.blksize);


        uint8_t status = VIRTIO_BLK_S_UNSUPP;

        if (virtq_sendrecv(driver, 0, &request, sizeof(request.hdr) + device->blk.blksize, &status, sizeof(uint8_t)) < 0) {
            return errno = EIO, -1;
        }

        switch (status) {
            case VIRTIO_BLK_S_OK:
                break;
            case VIRTIO_BLK_S_IOERR:
                return errno = EIO, -1;
            case VIRTIO_BLK_S_UNSUPP:
                return errno = ENOSYS, -1;
            default:
                kpanicf("virtio-blk: unknown status %d\n", status);
        }

        buf += device->blk.blksize;
    }

    return count;
}



static int virtblk_dev_init(struct virtio_driver* driver, size_t index) {

    DEBUG_ASSERT(driver);
    DEBUG_ASSERT(index < 26);


    device_t* device = kcalloc(sizeof(device_t), 1, GFP_KERNEL);

    device->type = DEVICE_TYPE_BLOCK;

    strncpy(device->name, "vda", DEVICE_MAXNAMELEN);
    strncpy(device->description, "VIRTIO Block Device", DEVICE_MAXDESCLEN);

    device->name[2] = 'a' + index;
    device->name[3] = '\0';

    device->major  = 3;
    device->minor  = 0;
    device->status = DEVICE_STATUS_UNKNOWN;
    device->init   = &virtblk_init;
    device->dnit   = &virtblk_dnit;
    device->reset  = &virtblk_reset;

    device->blk.write = &virtblk_write;
    device->blk.read  = &virtblk_read;

    device->userdata = (void*)driver;

    device_mkdev(device, 0660);

    return 0;
}

static int negotiate_features(struct virtio_driver* driver, uint32_t* features, size_t index) {

    if (index == 0) {

        *features |= VIRTIO_BLK_F_SIZE_MAX;
        *features |= VIRTIO_BLK_F_BLK_SIZE;
        *features |= VIRTIO_BLK_F_SEG_MAX;

        *features &= ~VIRTIO_BLK_F_MQ;
        *features &= ~VIRTIO_BLK_F_FLUSH;
        *features &= ~VIRTIO_BLK_F_ZONED;
    }

    return 0;
}

static int setup_config(struct virtio_driver* driver, uintptr_t device_config) {
    return 0;
}

static int interrupt_handler(pcidev_t device, irq_t vector, struct virtio_driver* driver) {
    return 0;
}


static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {

    if (vid != VIRTIO_PCI_VENDOR)
        return;

    if (did != VIRTIO_PCI_DEVICE(VIRTIO_DEVICE_TYPE_BLOCK))
        return;


    struct virtio_driver* driver = kcalloc(sizeof(struct virtio_driver), 1, GFP_KERNEL);

    driver->type             = VIRTIO_DEVICE_TYPE_BLOCK;
    driver->device           = device;
    driver->send_window_size = 4096;
    driver->recv_window_size = 4096;

    driver->negotiate = &negotiate_features;
    driver->setup     = &setup_config;
    driver->interrupt = &interrupt_handler;


    if (virtio_pci_init(driver) < 0) {

#if DEBUG_LEVEL_ERROR
        kprintf("virtio-blk: device %d (%X:%X) initialization failed\n", device, vid, did);
#endif

        return;
    }


    static size_t virtblk_dev_index = 0;

    if (virtblk_dev_init(driver, virtblk_dev_index++) < 0) {

#if DEBUG_LEVEL_ERROR
        kprintf("virtio-blk: device %d (%X:%X) initialization failed\n", device, vid, did);
#endif

        return;
    }

    kprintf("virtio-blk: device #%zd %d (%X:%X) initialized\n", virtblk_dev_index, device, vid, did);
}


void init(const char* args) {

    if (strstr(core->boot.cmdline, "virtio=off"))
        return;

    pci_scan(&pci_find, PCI_TYPE_ALL, NULL);
}

void dnit(void) {
}
