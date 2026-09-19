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
#include <stdio.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/endian.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include <dev/interface.h>
#include <dev/network.h>
#include <dev/pci.h>

#include <dev/virtio/virtio-net.h>
#include <dev/virtio/virtio.h>


MODULE_NAME("virtio/virtio-net");
MODULE_DEPS("dev/interface,dev/network,dev/pci,virtio/virtio-pci,virtio/virtio-queue");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


/**
 * @brief How many cards this driver claims.
 */
#define VIRTIO_NET_MAX_DEVICES 4

/**
 * @brief How many receive buffers are handed to the device, capped by what the queue has.
 */
#define VIRTIO_NET_BUFFERS 64

/**
 * @brief An ethernet header and an MTU of frame, which is the most a transmit can carry.
 */
#define VIRTIO_NET_FRAME_MAX 1514

/**
 * @brief Room for a frame and the header in front of it, rounded up.
 */
#define VIRTIO_NET_WINDOW_SIZE 2048

#define VIRTIO_NET_HDR_SIZE (sizeof(struct virtio_net_hdr))

/**
 * @brief The transitional network device, which answers to an identifier of its own.
 */
#define VIRTIO_NET_PCI_DEVICE_LEGACY 0x1000


struct virtnet {

    struct virtio_driver* driver;

    /* Set once the interface is up, which is what tells the interrupt that a frame may be
       handed to lwIP rather than dropped. */
    bool running;

    struct {
        uint16_t desc;
        uint16_t length;
        uint16_t offset;
    } rx;

    /* The frame being assembled out of a pbuf chain, header included, which endoutput hands
       to the device in one descriptor. */
    struct {
        uint8_t frame[VIRTIO_NET_HDR_SIZE + VIRTIO_NET_FRAME_MAX];
        uint16_t length;
    } tx;

    device_t device;
};


static struct virtnet* devices[VIRTIO_NET_MAX_DEVICES] = {0};

static size_t num_devices = 0;

/* What negotiate_features() settled on for the device being brought up, read by pci_find()
   as soon as virtio_pci_init() returns. */
static uint32_t negotiated_features = 0;


/**
 * @brief Accepts the address and the link state, and nothing this driver cannot honour.
 *
 * @param driver The driver owning the device.
 * @param features The features to accept, already cut down to what the device offers.
 * @param index The feature word being negotiated.
 * @return 0 on success, or a negative errno.
 */

static int negotiate_features(struct virtio_driver* driver, uint32_t* features, size_t index) {

    DEBUG_ASSERT(features);

    if (index != 0)
        return 0;

    *features &= (VIRTIO_NET_F_MAC | VIRTIO_NET_F_STATUS);

    negotiated_features = *features;

    return 0;
}


static int setup_config(struct virtio_driver* driver, uintptr_t device_config) {
    return 0;
}


/**
 * @brief Stocks the receive queue, which the device drops frames on the floor while empty.
 *
 * @param vn The card to stock.
 * @return The number of buffers handed to the device.
 */

static size_t virtnet_fill(struct virtnet* vn) {

    DEBUG_ASSERT(vn);

    const uint16_t q = VIRTIO_NET_QUEUE_RX;

    size_t posted = 0;

    for (size_t i = 0; i < VIRTIO_NET_BUFFERS; i++) {

        uint16_t desc = virtq_alloc_descriptor(vn->driver, q, VIRTQ_REQUEST_POSTED);

        if (desc == VIRTQ_DESC_NONE)
            break;

        virtq_provide(vn->driver, q, desc, vn->driver->recv_window_size);

        posted++;
    }

    if (posted)
        virtq_notify(vn->driver, q);

#if DEBUG_LEVEL_TRACE
    kprintf("virtio-net: device %d stocked the receive queue with %d buffers\n", vn->driver->device, posted);
#endif

    return posted;
}


static void virtnet_low_level_init(void* internals, uint8_t* address, void* mcast) {

    DEBUG_ASSERT(internals);
    DEBUG_ASSERT(address);

    struct virtnet* vn = (struct virtnet*)internals;

    if (unlikely(!virtnet_fill(vn))) {
#if DEBUG_LEVEL_ERROR
        kprintf("virtio-net: ERROR! device %d has no room in the receive queue\n", vn->driver->device);
#endif
    }
}


static int virtnet_startoutput(void* internals) {

    DEBUG_ASSERT(internals);

    struct virtnet* vn = (struct virtnet*)internals;

    vn->tx.length = 0;

    return 1;
}


/**
 * @brief Appends one segment of the frame behind the header, dropping whatever will not fit.
 *
 * @param internals The card to transmit on.
 * @param buf The segment.
 * @param len The length of the segment.
 */

static void virtnet_output(void* internals, void* buf, uint16_t len) {

    DEBUG_ASSERT(internals);
    DEBUG_ASSERT(buf);

    struct virtnet* vn = (struct virtnet*)internals;

    if (unlikely(vn->tx.length >= VIRTIO_NET_FRAME_MAX))
        return;

    if (unlikely(vn->tx.length + len > VIRTIO_NET_FRAME_MAX))
        len = VIRTIO_NET_FRAME_MAX - vn->tx.length;

    memcpy(&vn->tx.frame[VIRTIO_NET_HDR_SIZE + vn->tx.length], buf, len);

    vn->tx.length += len;
}


/**
 * @brief Hands the assembled frame to the device, with a header asking for no offloading.
 *
 * @param internals The card to transmit on.
 * @param len The length lwIP counted, which the assembled frame is trusted over.
 */

static void virtnet_endoutput(void* internals, uint16_t len) {

    DEBUG_ASSERT(internals);

    struct virtnet* vn = (struct virtnet*)internals;

    if (unlikely(!vn->tx.length))
        return;

    memset(&vn->tx.frame[0], 0, VIRTIO_NET_HDR_SIZE);

    ssize_t e = virtq_send(vn->driver, VIRTIO_NET_QUEUE_TX, vn->tx.frame, VIRTIO_NET_HDR_SIZE + vn->tx.length);

    vn->tx.length = 0;

    if (unlikely(e < 0)) {

        LINK_STATS_INC(link.drop);

#if DEBUG_LEVEL_ERROR
        kprintf("virtio-net: ERROR! device %d could not transmit a %d byte frame (%d)\n", vn->driver->device, len, (int)e);
#endif
    }
}


static int virtnet_startinput(void* internals) {

    DEBUG_ASSERT(internals);

    struct virtnet* vn = (struct virtnet*)internals;

    return vn->rx.length;
}


/**
 * @brief Copies the next piece of the frame the device wrote, skipping the header in front of it.
 *
 * @param internals The card to receive from.
 * @param buf The segment to fill.
 * @param len The length of the segment.
 */

static void virtnet_input(void* internals, void* buf, uint16_t len) {

    DEBUG_ASSERT(internals);
    DEBUG_ASSERT(buf);

    struct virtnet* vn = (struct virtnet*)internals;

    DEBUG_ASSERT(vn->rx.desc != VIRTQ_DESC_NONE);

    if (unlikely(vn->rx.offset >= vn->rx.length))
        return;

    if (unlikely(vn->rx.offset + len > vn->rx.length))
        len = vn->rx.length - vn->rx.offset;

    const uint8_t* frame = (const uint8_t*)virtq_recvbuf(vn->driver, VIRTIO_NET_QUEUE_RX, vn->rx.desc) + VIRTIO_NET_HDR_SIZE;

    memcpy(buf, frame + vn->rx.offset, len);

    vn->rx.offset += len;
}


static void virtnet_endinput(void* internals) {

    DEBUG_ASSERT(internals);

    struct virtnet* vn = (struct virtnet*)internals;

    vn->rx.length = 0;
    vn->rx.offset = 0;
}


static void virtnet_input_nomem(void* internals, uint16_t len) {

    DEBUG_ASSERT(internals);

#if DEBUG_LEVEL_ERROR
    kprintf("virtio-net: ERROR! no memory left for a %d byte frame, dropping it\n", len);
#endif
}


/**
 * @brief Hands every frame the device has finished writing to lwIP and puts the buffers straight back.
 *
 * @param vn The card to drain.
 */

static void virtnet_receive(struct virtnet* vn) {

    DEBUG_ASSERT(vn);

    struct virtio_driver* driver = vn->driver;

    const uint16_t q = VIRTIO_NET_QUEUE_RX;

    bool recycled = false;

    uint16_t desc;
    uint32_t len;

    while (virtq_reap(driver, q, &desc, &len)) {

        if (desc == VIRTQ_DESC_NONE)
            continue;

        if (likely(vn->running && len > VIRTIO_NET_HDR_SIZE)) {

            vn->rx.desc   = desc;
            vn->rx.length = (uint16_t)MIN(len - VIRTIO_NET_HDR_SIZE, driver->recv_window_size - VIRTIO_NET_HDR_SIZE);
            vn->rx.offset = 0;

            ethif_input(&vn->device.net.interface);
        }

        vn->rx.desc   = VIRTQ_DESC_NONE;
        vn->rx.length = 0;
        vn->rx.offset = 0;

        virtq_provide(driver, q, desc, driver->recv_window_size);

        recycled = true;
    }

    if (recycled)
        virtq_notify(driver, q);
}


static int interrupt_handler(pcidev_t device, irq_t vector, struct virtio_driver* driver) {

    for (size_t i = 0; i < num_devices; i++) {

        if (devices[i]->driver != driver)
            continue;

        while (virtq_reap(driver, VIRTIO_NET_QUEUE_TX, NULL, NULL))
            ;

        virtnet_receive(devices[i]);
    }

    return 0;
}


/**
 * @brief Reads the address the device was given, making one up when it will not say.
 *
 * @param vn The card to address.
 * @param cfg The device configuration window, which a device need not have.
 */

static void virtnet_read_address(struct virtnet* vn, struct virtio_net_config volatile* cfg) {

    DEBUG_ASSERT(vn);

    if (cfg && (negotiated_features & VIRTIO_NET_F_MAC)) {

        for (size_t i = 0; i < ETHARP_HWADDR_LEN; i++)
            vn->device.net.address[i] = mmio_r8(&cfg->mac[i]);

        return;
    }

    static const uint8_t fallback[ETHARP_HWADDR_LEN] = {0x02, 0x41, 0x50, 0x4C, 0x55, 0x53};

    memcpy(vn->device.net.address, fallback, ETHARP_HWADDR_LEN);

    vn->device.net.address[ETHARP_HWADDR_LEN - 1] += (uint8_t)num_devices;
}


static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {

    if (num_devices >= VIRTIO_NET_MAX_DEVICES)
        return;

    if (vid != VIRTIO_PCI_VENDOR)
        return;

    if (did != VIRTIO_PCI_DEVICE(VIRTIO_DEVICE_TYPE_NETWORK) && did != VIRTIO_NET_PCI_DEVICE_LEGACY)
        return;


    struct virtio_driver* driver = kcalloc(1, sizeof(struct virtio_driver), GFP_KERNEL);

    if (unlikely(!driver))
        return;

    driver->type   = VIRTIO_DEVICE_TYPE_NETWORK;
    driver->device = device;

    driver->send_window_size = VIRTIO_NET_WINDOW_SIZE;
    driver->recv_window_size = VIRTIO_NET_WINDOW_SIZE;
    driver->max_queues       = 2;

    driver->negotiate = &negotiate_features;
    driver->setup     = &setup_config;
    driver->interrupt = &interrupt_handler;

    negotiated_features = 0;


    if (virtio_pci_init(driver) < 0) {
#if DEBUG_LEVEL_ERROR
        kprintf("virtio-net: device %d (%X:%X) initialization failed\n", device, vid, did);
#endif
        kfree(driver);
        return;
    }


    if (unlikely(driver->internals.num_queues < 2)) {
#if DEBUG_LEVEL_ERROR
        kprintf("virtio-net: ERROR! device %d offers %d queues, too few to send and receive on\n", device, driver->internals.num_queues);
#endif
        virtio_pci_dnit(driver);
        kfree(driver);
        return;
    }


    struct virtnet* vn = kcalloc(1, sizeof(struct virtnet), GFP_KERNEL);

    if (unlikely(!vn)) {

        virtio_pci_dnit(driver);
        kfree(driver);

        return;
    }

    vn->driver  = driver;
    vn->running = false;

    vn->rx.desc = VIRTQ_DESC_NONE;


    struct virtio_net_config volatile* cfg = (struct virtio_net_config volatile*)driver->internals.device_config;

    virtnet_read_address(vn, cfg);


    if (num_devices == 0)
        strncpy(vn->device.name, "virtio-net", DEVICE_MAXNAMELEN - 1);
    else
        snprintf(vn->device.name, DEVICE_MAXNAMELEN, "virtio-net%d", (int)num_devices);

    strncpy(vn->device.description, "VIRTIO Network Device", DEVICE_MAXDESCLEN - 1);

    vn->device.major  = 144;
    vn->device.minor  = 2 + num_devices;
    vn->device.type   = DEVICE_TYPE_NETWORK;
    vn->device.status = DEVICE_STATUS_UNKNOWN;

    vn->device.net.low_level_init        = virtnet_low_level_init;
    vn->device.net.low_level_startoutput = virtnet_startoutput;
    vn->device.net.low_level_output      = virtnet_output;
    vn->device.net.low_level_endoutput   = virtnet_endoutput;
    vn->device.net.low_level_startinput  = virtnet_startinput;
    vn->device.net.low_level_input       = virtnet_input;
    vn->device.net.low_level_endinput    = virtnet_endinput;
    vn->device.net.low_level_input_nomem = virtnet_input_nomem;

    IP4_ADDR(&vn->device.net.ip, 10, 0, 2, 15 + num_devices);
    IP4_ADDR(&vn->device.net.nm, 255, 255, 255, 0);
    IP4_ADDR(&vn->device.net.gw, 10, 0, 2, 2);

    vn->device.net.interface.state = &vn->device;
    vn->device.net.internals       = vn;


    devices[num_devices] = vn;

    if (!netif_add(&vn->device.net.interface, &vn->device.net.ip, &vn->device.net.nm, &vn->device.net.gw, &vn->device, ethif_init, ethernet_input)) {

        devices[num_devices] = NULL;

        virtio_pci_dnit(driver);

        kfree(vn);
        kfree(driver);

        kpanicf("virtio-net: PANIC! netif_add() failed\n");
    }

    num_devices++;


    netif_set_default(&vn->device.net.interface);
    netif_set_up(&vn->device.net.interface);

    if (cfg && (negotiated_features & VIRTIO_NET_F_STATUS) && !(le16_to_cpu(mmio_r16(&cfg->status)) & VIRTIO_NET_S_LINK_UP))
        netif_set_link_down(&vn->device.net.interface);
    else
        netif_set_link_up(&vn->device.net.interface);

    device_mkdev(&vn->device, 0666);

    vn->running = true;


#if DEBUG_LEVEL_INFO
    kprintf("virtio-net: device %d ready as %s [mac(%02X:%02X:%02X:%02X:%02X:%02X), queues(%d)]\n", device, vn->device.name, vn->device.net.address[0], vn->device.net.address[1], vn->device.net.address[2], vn->device.net.address[3],
            vn->device.net.address[4], vn->device.net.address[5], driver->internals.num_queues);
#endif
}


void init(const char* args) {

    if (strstr(core->boot.cmdline, "virtio=off"))
        return;

    if (strstr(core->boot.cmdline, "network=off"))
        return;

    pci_scan(&pci_find, PCI_TYPE_ALL, NULL);
}

void dnit(void) {

    for (size_t i = 0; i < num_devices; i++)
        device_unlink(&devices[i]->device);
}
