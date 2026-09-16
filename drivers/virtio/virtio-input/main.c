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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/endian.h>
#include <aplus/errno.h>
#include <aplus/events.h>
#include <aplus/hal.h>
#include <aplus/input.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <dev/char.h>
#include <dev/interface.h>
#include <dev/pci.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtio-input.h>


MODULE_NAME("virtio/virtio-input");
MODULE_DEPS("dev/interface,dev/char,dev/pci,virtio/virtio-pci,virtio/virtio-queue");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


/**
 * @brief How many absolute pointing devices this driver claims; relative and key devices are left alone.
 */


#define VIRTIO_INPUT_MAX_DEVICES 4

/**
 * @brief As much of the event queue as it will give, capped by what the queue actually has.
 */
#define VIRTIO_INPUT_BUFFERS 64

#define VIRTIO_INPUT_EVENT_SIZE (sizeof(struct virtio_input_event))


struct virtinput {

    struct virtio_driver* driver;
    device_t* device;

    uint16_t buffers;

    /* The range the device reports its axes in, which is whatever it likes: X and Y are
       scaled onto EV_ABS_MAX on the way out so that a reader needs to know only the screen
       size to place the pointer. */
    struct {
        uint32_t min;
        uint32_t range;
    } abs[2];

    /* The position being assembled out of the report in progress, and whether anything in
       this report has changed it. */
    struct {
        vaxis_t x;
        vaxis_t y;
        bool moved;
    } pointer;
};


static struct virtinput* devices[VIRTIO_INPUT_MAX_DEVICES] = {0};
static device_t chardevs[VIRTIO_INPUT_MAX_DEVICES]         = {0};

static size_t num_devices = 0;


/**
 * @brief Selects an entry of the device configuration window.
 *
 * @param cfg The configuration window.
 * @param select The entry to select.
 * @param subsel The subentry to select.
 * @return How many bytes of the entry the device filled in, zero when it has nothing to say about it.
 */

static uint8_t virtinput_cfg_select(struct virtio_input_config volatile* cfg, uint8_t select, uint8_t subsel) {

    DEBUG_ASSERT(cfg);

    mmio_w8(&cfg->select, select);
    mmio_w8(&cfg->subsel, subsel);

    atomic_thread_fence(memory_order_seq_cst);

    return mmio_r8(&cfg->size);
}


static bool virtinput_cfg_has_bit(struct virtio_input_config volatile* cfg, uint8_t select, uint8_t subsel, uint16_t bit) {

    uint8_t size = virtinput_cfg_select(cfg, select, subsel);

    if (bit / 8 >= size)
        return false;

    return !!(mmio_r8(&cfg->u.bitmap[bit / 8]) & (1 << (bit % 8)));
}


/**
 * @brief Stocks the event queue, which the device needs whole before it reports anything.
 *
 * @param vi The device to stock.
 * @return 0 on success, or a negative errno.
 */

static int virtinput_fill(struct virtinput* vi) {

    DEBUG_ASSERT(vi);

    for (size_t i = 0; i < VIRTIO_INPUT_BUFFERS; i++) {

        uint16_t desc = virtq_alloc_descriptor(vi->driver, VIRTIO_INPUT_QUEUE_EVENT, VIRTQ_REQUEST_POSTED);

        if (desc == VIRTQ_DESC_NONE)
            break;

        virtq_provide(vi->driver, VIRTIO_INPUT_QUEUE_EVENT, desc, VIRTIO_INPUT_EVENT_SIZE);

        vi->buffers++;
    }

    if (unlikely(!vi->buffers))
        return errno = ENOSPC, -1;

    virtq_notify(vi->driver, VIRTIO_INPUT_QUEUE_EVENT);

    return 0;
}


static void virtinput_write(struct virtinput* vi, event_t* ev) {

    ev->ev_devid = (vi->device->major << 16) | (vi->device->minor & 0xFFFF);

    vfs_write(vi->device->inode, ev, 0, sizeof(*ev));
}


/**
 * @brief Publishes the position the report in progress has assembled, if it moved the pointer.
 *
 * @param vi The device whose report has closed.
 */

static void virtinput_flush_pointer(struct virtinput* vi) {

    DEBUG_ASSERT(vi);

    if (!vi->pointer.moved)
        return;

    vi->pointer.moved = false;


    event_t ev = {};

    ev.ev_type  = EV_ABS;
    ev.ev_abs.x = vi->pointer.x;
    ev.ev_abs.y = vi->pointer.y;
    ev.ev_abs.z = 0;

    virtinput_write(vi, &ev);
}


/**
 * @brief Translates one device event into the kernel's own and publishes it.
 *
 * @param vi The device the event came from.
 * @param in The event to translate.
 */

static void virtinput_publish(struct virtinput* vi, const struct virtio_input_event* in) {

    DEBUG_ASSERT(vi);
    DEBUG_ASSERT(in);

    uint16_t type  = le16_to_cpu(in->type);
    uint16_t code  = le16_to_cpu(in->code);
    uint32_t value = le32_to_cpu(in->value);


    event_t ev = {};

    switch (type) {

        case EV_SYN:

            if (code == SYN_REPORT)
                virtinput_flush_pointer(vi);

            return;


        case EV_ABS: {

            if (code != ABS_X && code != ABS_Y)
                return;

            const uint32_t min   = vi->abs[code].min;
            const uint32_t range = vi->abs[code].range;

            uint32_t v = value < min ? 0 : value - min;

            if (v > range)
                v = range;

            vaxis_t scaled = (vaxis_t)(((uint64_t)v * EV_ABS_MAX) / range);

            if (code == ABS_X) {

                if (scaled == vi->pointer.x)
                    return;

                vi->pointer.x = scaled;

            } else {

                if (scaled == vi->pointer.y)
                    return;

                vi->pointer.y = scaled;
            }

            vi->pointer.moved = true;

            return;
        }


        case EV_KEY:

            virtinput_flush_pointer(vi);

            ev.ev_type     = EV_KEY;
            ev.ev_key.vkey = code;
            ev.ev_key.down = value ? 1 : 0;

            break;


        case EV_REL:

            if (code != REL_WHEEL)
                return;

            virtinput_flush_pointer(vi);

            ev.ev_type  = EV_REL;
            ev.ev_rel.x = 0;
            ev.ev_rel.y = 0;
            ev.ev_rel.z = (vaxis_t)(int32_t)value;

            break;


        default:
            return;
    }

    virtinput_write(vi, &ev);
}


/**
 * @brief Drains everything the device has finished writing and puts the buffers straight back.
 *
 * @param vi The device to drain.
 */

static void virtinput_drain(struct virtinput* vi) {

    DEBUG_ASSERT(vi);

    struct virtio_driver* driver = vi->driver;

    const uint16_t q = VIRTIO_INPUT_QUEUE_EVENT;

    bool recycled = false;

    uint16_t desc;
    uint32_t len;

    while (virtq_reap(driver, q, &desc, &len)) {

        if (desc == VIRTQ_DESC_NONE)
            continue;

        if (likely(len >= VIRTIO_INPUT_EVENT_SIZE)) {

            struct virtio_input_event in;

            memcpy(&in, (const void*)virtq_recvbuf(driver, q, desc), sizeof(in));

            virtinput_publish(vi, &in);
        }

        virtq_provide(driver, q, desc, VIRTIO_INPUT_EVENT_SIZE);

        recycled = true;
    }

    if (recycled)
        virtq_notify(driver, q);
}


static int interrupt_handler(pcidev_t device, irq_t vector, struct virtio_driver* driver) {

    for (size_t i = 0; i < num_devices; i++) {

        if (devices[i]->driver == driver)
            virtinput_drain(devices[i]);
    }

    return 0;
}


static int setup_features(struct virtio_driver* driver, uint32_t* features, size_t index) {

    if (index == 1)
        *features |= VIRTIO_F_VERSION_1;

    return 0;
}


static int setup_config(struct virtio_driver* driver, uintptr_t device_config) {
    return 0;
}


/**
 * @brief Reads the range of one absolute axis, refusing an axis the device gives no range.
 *
 * @param vi The device to read from.
 * @param cfg The configuration window.
 * @param axis The axis to read.
 * @return 0 on success, or a negative errno.
 */

static int virtinput_abs_range(struct virtinput* vi, struct virtio_input_config volatile* cfg, uint8_t axis) {

    if (virtinput_cfg_select(cfg, VIRTIO_INPUT_CFG_ABS_INFO, axis) < sizeof(struct virtio_input_absinfo))
        return errno = ENOTSUP, -1;

    uint32_t min = le32_to_cpu(mmio_r32(&cfg->u.abs.min));
    uint32_t max = le32_to_cpu(mmio_r32(&cfg->u.abs.max));

    if (max <= min)
        return errno = ENOTSUP, -1;

    vi->abs[axis].min   = min;
    vi->abs[axis].range = max - min;

    return 0;
}


static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {

    if (num_devices >= VIRTIO_INPUT_MAX_DEVICES)
        return;

    if (vid != VIRTIO_PCI_VENDOR)
        return;

    if (did != VIRTIO_PCI_DEVICE(VIRTIO_DEVICE_TYPE_INPUT))
        return;


    struct virtio_driver* driver = kcalloc(1, sizeof(struct virtio_driver), GFP_KERNEL);

    if (unlikely(!driver))
        return;

    driver->type   = VIRTIO_DEVICE_TYPE_INPUT;
    driver->device = device;

    driver->send_window_size = 64;
    driver->recv_window_size = 64;
    driver->max_queues       = 2;

    driver->negotiate = &setup_features;
    driver->setup     = &setup_config;
    driver->interrupt = &interrupt_handler;


    if (virtio_pci_init(driver) < 0) {

#if DEBUG_LEVEL_ERROR
        kprintf("virtio-input: device %d (%X:%X) initialization failed\n", device, vid, did);
#endif

        kfree(driver);
        return;
    }


    struct virtio_input_config volatile* cfg = (struct virtio_input_config volatile*)driver->internals.device_config;

    if (unlikely(!cfg)) {

#if DEBUG_LEVEL_ERROR
        kprintf("virtio-input: ERROR! device %d has no device configuration to identify it by\n", device);
#endif

        virtio_pci_dnit(driver);
        kfree(driver);

        return;
    }


    if (!virtinput_cfg_has_bit(cfg, VIRTIO_INPUT_CFG_EV_BITS, EV_ABS, ABS_X) || !virtinput_cfg_has_bit(cfg, VIRTIO_INPUT_CFG_EV_BITS, EV_ABS, ABS_Y)) {

#if DEBUG_LEVEL_WARN
        kprintf("virtio-input: WARN! device %d is not an absolute pointer, leaving it unread\n", device);
#endif

        virtio_pci_dnit(driver);
        kfree(driver);

        return;
    }


    struct virtinput* vi = kcalloc(1, sizeof(struct virtinput), GFP_KERNEL);

    if (unlikely(!vi))
        return;

    vi->driver = driver;


    if (virtinput_abs_range(vi, cfg, ABS_X) < 0 || virtinput_abs_range(vi, cfg, ABS_Y) < 0) {

#if DEBUG_LEVEL_ERROR
        kprintf("virtio-input: device %d reports absolute axes without a usable range\n", device);
#endif

        virtio_pci_dnit(driver);

        kfree(vi);
        kfree(driver);

        return;
    }


    device_t* chr = &chardevs[num_devices];

    chr->type = DEVICE_TYPE_CHAR;

    if (num_devices == 0)
        strncpy(chr->name, "tablet", DEVICE_MAXNAMELEN - 1);
    else
        snprintf(chr->name, DEVICE_MAXNAMELEN, "tablet%d", (int)num_devices);

    strncpy(chr->description, "VIRTIO absolute pointer input device", DEVICE_MAXDESCLEN - 1);

    chr->major = 13;
    chr->minor = 64 + num_devices;

    chr->status = DEVICE_STATUS_UNKNOWN;

    chr->chr.io = CHAR_IO_FBF;

    chr->userdata = vi;

    vi->device = chr;


    devices[num_devices++] = vi;

    device_mkdev(chr, 0666);


    if (virtinput_fill(vi) < 0) {

#if DEBUG_LEVEL_ERROR
        kprintf("virtio-input: device %d has no room in the event queue\n", device);
#endif

        return;
    }


#if DEBUG_LEVEL_TRACE
    kprintf("virtio-input: device %d is an absolute pointer [x(%d..%d), y(%d..%d), buffers(%d)]\n", device, vi->abs[ABS_X].min, vi->abs[ABS_X].min + vi->abs[ABS_X].range, vi->abs[ABS_Y].min,
            vi->abs[ABS_Y].min + vi->abs[ABS_Y].range, vi->buffers);
#endif
}


void init(const char* args) {

    if (strstr(core->boot.cmdline, "virtio=off"))
        return;

    pci_scan(&pci_find, PCI_TYPE_ALL, NULL);
}

void dnit(void) {

    for (size_t i = 0; i < num_devices; i++)
        device_unlink(devices[i]->device);
}
