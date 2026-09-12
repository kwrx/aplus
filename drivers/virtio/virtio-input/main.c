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


/* This driver claims absolute pointing devices -- virtio-tablet-pci and nothing else. The
 * point of it is not that a tablet is a nicer mouse: it is that a relative pointer forces
 * the host to grab the real one to deliver motion, and a grabbed host pointer is hidden,
 * which takes the adapter's cursor plane down with it. An absolute pointer needs no grab, so
 * the plane the virtio-gpu driver hands the host is the pointer the user actually sees.
 *
 * Relative and key devices are left to whoever already owns /dev/mouse and /dev/kbd: this
 * driver would only fight the PS/2 driver for those names, and the machines that have a
 * virtio tablet have a PS/2 keyboard too.
 *
 * Left, but not left alone: what a device is cannot be read before its BARs are mapped, and
 * mapping them means bringing it all the way up, which is the point at which the host starts
 * routing that kind of input to it. virtio_pci_init() has no counterpart to undo that, so a
 * virtio keyboard probed here would be brought up, abandoned, and would take the keyboard
 * away from the PS/2 driver by existing. Nothing configures one today -- scripts/run-qemu
 * adds the tablet and nothing else -- and the fix is to drive them rather than to probe more
 * carefully, so this is a limitation to know about rather than one to guard against.
 */


#define VIRTIO_INPUT_MAX_DEVICES 4

/* As much of the event queue as it will give. The device takes one buffer per event and
   drops a whole report the moment it cannot place all of it, so the only thing depth buys is
   headroom for reports that arrive faster than the interrupt drains them -- and there is
   nothing else for these descriptors to be spent on. The allocator caps this at what the
   queue actually has. */
#define VIRTIO_INPUT_BUFFERS 64

#define VIRTIO_INPUT_EVENT_SIZE (sizeof(struct virtio_input_event))


struct virtinput {

    struct virtio_driver* driver;
    device_t* device;

    /* Guards the available ring, which is written both while setting the device up and from
       the interrupt that recycles buffers the device has finished with. */
    spinlock_t lock;

    /* How far into the used ring this driver has looked. Kept as uint16_t so that it wraps
       exactly where the device's own index does. */
    uint16_t last_used;

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


/* Select an entry of the device configuration window and return how many bytes of it the
 * device filled in. Zero means the device has nothing to say about that entry, which is how
 * its capabilities are discovered: an absent EV_BITS/EV_ABS entry means no absolute axes.
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


/* Hand one buffer back to the device. The caller holds the lock; notifying is left to the
 * caller too, so that recycling a whole batch costs one notification rather than one each.
 */

static void virtinput_post(struct virtinput* vi, uint16_t desc) {

    DEBUG_ASSERT(vi);

    struct virtio_driver* driver = vi->driver;

    const uint16_t q = VIRTIO_INPUT_QUEUE_EVENT;

    driver->internals.queues[q].descriptors[desc].q_address = cpu_to_le64(driver->internals.queues[q].buffers.recvbuf + (desc * driver->recv_window_size));
    driver->internals.queues[q].descriptors[desc].q_length  = cpu_to_le32(VIRTIO_INPUT_EVENT_SIZE);
    driver->internals.queues[q].descriptors[desc].q_flags   = cpu_to_le16(VIRTQ_DESC_F_WRITE);
    driver->internals.queues[q].descriptors[desc].q_next    = cpu_to_le16(0);

    uint16_t next = le16_to_cpu(driver->internals.queues[q].available->q_idx) % driver->internals.queues[q].size;

    driver->internals.queues[q].available->q_ring[next] = cpu_to_le16(desc);
    driver->internals.queues[q].available->q_flags      = cpu_to_le16(0);
    driver->internals.queues[q].available->q_idx        = cpu_to_le16(le16_to_cpu(driver->internals.queues[q].available->q_idx) + 1);
}


static void virtinput_notify(struct virtinput* vi) {

    DEBUG_ASSERT(vi);

    atomic_thread_fence(memory_order_release);

    vi->driver->internals.queues[VIRTIO_INPUT_QUEUE_EVENT].notify->n_idx = cpu_to_le16(VIRTIO_INPUT_QUEUE_EVENT);
}


/* Fill the event queue. The device drops an entire report the moment it does not have room
 * for all of it at once, so the queue is stocked before anything can be reported rather than
 * a buffer at a time as events arrive.
 */

static int virtinput_fill(struct virtinput* vi) {

    DEBUG_ASSERT(vi);

    scoped_lock(&vi->lock) {

        for (size_t i = 0; i < VIRTIO_INPUT_BUFFERS; i++) {

            /* Zero is never handed out by the allocator, so it is how it reports that the
               queue has no descriptor left -- a partial fill is still a working device. */
            uint16_t desc = virtq_alloc_descriptor(vi->driver, VIRTIO_INPUT_QUEUE_EVENT);

            if (desc == 0)
                break;

            virtinput_post(vi, desc);

            vi->buffers++;
        }
    }

    if (unlikely(!vi->buffers))
        return errno = ENOSPC, -1;

    virtinput_notify(vi);

    return 0;
}


static void virtinput_write(struct virtinput* vi, event_t* ev) {

    ev->ev_devid = (vi->device->major << 16) | (vi->device->minor & 0xFFFF);

    vfs_write(vi->device->inode, ev, 0, sizeof(*ev));
}


/* Publish the position the report in progress has assembled, if it moved the pointer.
 *
 * The axes arrive one event at a time and event_t carries them as a pair, so sending one the
 * moment it lands would put the pointer somewhere it never was: a diagonal movement would
 * pass through the corner between the old Y and the new X. The pair is held until the report
 * that set it is closed, which is exactly what EV_SYN is for.
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


/* Translate one device event into the kernel's own and publish it. */

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

            /* Scale onto the fixed range the event interface promises. The device's own
               range is read once at probe time and is not required to start at zero. */
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

            /* Where the pointer was when the button was pressed is part of what a press
               means, so the position this report carries goes out ahead of it. */
            virtinput_flush_pointer(vi);

            ev.ev_type     = EV_KEY;
            ev.ev_key.vkey = code;
            ev.ev_key.down = value ? 1 : 0;

            break;


        case EV_REL:

            /* A tablet still has a wheel, and the wheel is relative even when the pointer
               is not. */
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


/* Drain everything the device has finished writing and put the buffers straight back. The
 * whole batch is recycled under one lock and announced with one notification.
 */

static void virtinput_drain(struct virtinput* vi) {

    DEBUG_ASSERT(vi);

    struct virtio_driver* driver = vi->driver;

    const uint16_t q = VIRTIO_INPUT_QUEUE_EVENT;

    bool recycled = false;

    while (vi->last_used != le16_to_cpu(driver->internals.queues[q].used->q_idx)) {

        atomic_thread_fence(memory_order_acquire);

        uint16_t i    = vi->last_used % driver->internals.queues[q].size;
        uint16_t desc = (uint16_t)le32_to_cpu(driver->internals.queues[q].used->q_elements[i].e_id);
        uint32_t len  = le32_to_cpu(driver->internals.queues[q].used->q_elements[i].e_length);

        vi->last_used++;

        if (unlikely(desc >= driver->internals.queues[q].size))
            continue;

        if (likely(len >= VIRTIO_INPUT_EVENT_SIZE)) {

            struct virtio_input_event in;

            memcpy(&in, (void*)arch_vmm_p2v(driver->internals.queues[q].buffers.recvbuf + (desc * driver->recv_window_size), ARCH_VMM_AREA_HEAP), sizeof(in));

            virtinput_publish(vi, &in);
        }

        scoped_lock(&vi->lock) {
            virtinput_post(vi, desc);
        }

        recycled = true;
    }

    if (recycled)
        virtinput_notify(vi);
}


static int interrupt_handler(pcidev_t device, irq_t vector, struct virtio_driver* driver) {

    /* The vector is only meaningful on the MSI-X path, and a drain with nothing to drain is
       a single load of the used index, so every device on this driver is simply checked. */
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


/* Read the range of one absolute axis. A device that reports the axis but gives it no range
 * would make the scaling a division by zero, so an empty range is refused here rather than
 * guarded against on every event.
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

    driver->type   = VIRTIO_DEVICE_TYPE_INPUT;
    driver->device = device;

    /* One event per buffer is all the device ever writes, but a queue window is also the
       stride between buffers, so it is kept at a size the allocator rounds sensibly. */
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


    /* Only absolute pointers are claimed. Anything else is a device the PS/2 driver is
       already serving under the name a client would look for. */
    if (!virtinput_cfg_has_bit(cfg, VIRTIO_INPUT_CFG_EV_BITS, EV_ABS, ABS_X) || !virtinput_cfg_has_bit(cfg, VIRTIO_INPUT_CFG_EV_BITS, EV_ABS, ABS_Y)) {

#if DEBUG_LEVEL_WARN
        kprintf("virtio-input: WARN! device %d is not an absolute pointer, leaving it unread\n", device);
#endif

        /* The driver struct outlives this function on purpose: the device is up and its
           interrupt handler still points here. Freeing it would leave the handler holding a
           pointer to nothing. It finds no match in devices[] and returns, which is all that
           is wanted of it. */
        return;
    }


    struct virtinput* vi = kcalloc(1, sizeof(struct virtinput), GFP_KERNEL);

    vi->driver = driver;

    spinlock_init(&vi->lock);


    if (virtinput_abs_range(vi, cfg, ABS_X) < 0 || virtinput_abs_range(vi, cfg, ABS_Y) < 0) {

#if DEBUG_LEVEL_ERROR
        kprintf("virtio-input: device %d reports absolute axes without a usable range\n", device);
#endif

        kfree(vi);
        return;
    }


    device_t* chr = &chardevs[num_devices];

    chr->type = DEVICE_TYPE_CHAR;

    strncpy(chr->name, "tablet", DEVICE_MAXNAMELEN - 1);
    strncpy(chr->description, "VIRTIO absolute pointer input device", DEVICE_MAXDESCLEN - 1);

    /* Major 13 is the input family; 64 upwards is where its event devices live. */
    chr->major = 13;
    chr->minor = 64 + num_devices;

    chr->status = DEVICE_STATUS_UNKNOWN;

    chr->chr.io = CHAR_IO_FBF;

    chr->userdata = vi;

    vi->device = chr;


    devices[num_devices++] = vi;

    device_mkdev(chr, 0666);


    /* Only now: a buffer handed over before the device node exists would let an event
       arrive with nowhere to publish it. */
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
