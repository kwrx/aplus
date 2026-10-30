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
#include <aplus/events.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/task.h>
#include <aplus/vfs.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/sysmacros.h>

#include <dev/char.h>
#include <dev/interface.h>
#include <dev/usb/hid.h>
#include <dev/usb/usb.h>


MODULE_NAME("usb/hid");
MODULE_DEPS("dev/interface,dev/char,usb/core");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#define USB_CLASS_HID            0x03
#define HID_SUBCLASS_BOOT        1
#define HID_REQUEST_SET_IDLE     0x0a
#define HID_REQUEST_SET_PROTOCOL 0x0b
#define HID_BOOT_PROTOCOL        0
#define HID_QUEUE_DEPTH          16
#define HID_MAX_HANDLERS         4


/** @brief One boot-protocol interface and the handler turning its reports into events. */
typedef struct {
    void* queue;
    endpoint_t* ep;
    size_t size;
    int protocol;
    const struct usb_hid_handler* handler;
    void* ctx;
} usbhid_t;

#define HID_INST(dev) ((usbhid_t*)(dev)->data)


static const struct usb_hid_handler* _Atomic handlers[HID_MAX_HANDLERS];



/**
 * @brief Finds the handler registered for a boot protocol.
 * @param protocol The protocol.
 * @return The handler, or NULL if there is none yet.
 */
static const struct usb_hid_handler* usb_hid_handler(int protocol) {

    for (size_t i = 0; i < HID_MAX_HANDLERS; i++) {

        const struct usb_hid_handler* h = atomic_load(&handlers[i]);

        if (h && h->protocol == protocol)
            return h;
    }

    return NULL;
}


int usb_hid_register(const struct usb_hid_handler* handler) {

    DEBUG_ASSERT(handler);

    if (usb_hid_handler(handler->protocol))
        return -1;

    for (size_t i = 0; i < HID_MAX_HANDLERS; i++) {

        const struct usb_hid_handler* none = NULL;

        if (atomic_compare_exchange_strong(&handlers[i], &none, handler))
            return 0;
    }

    return -1;
}


void usb_hid_unregister(const struct usb_hid_handler* handler) {

    for (size_t i = 0; i < HID_MAX_HANDLERS; i++) {

        const struct usb_hid_handler* h = handler;

        atomic_compare_exchange_strong(&handlers[i], &h, NULL);
    }
}



void usb_hid_sink_open(struct usb_hid_sink* sink, const char* path) {

    inode_t* root = NULL;

    shared_ptr_access(current_task->fs, fs, { root = vfs_inode_get(fs->root); });

    if (likely(root)) {
        sink->inode = path_lookup(root, path, 0, 0);
        vfs_inode_put(root);
    }

    if (sink->inode) {

        struct stat st;

        if (vfs_getattr(sink->inode, &st) == 0)
            sink->devid = (major(st.st_rdev) << 16) | (minor(st.st_rdev) & 0xFFFF);

        return;
    }

    device_mkdev(&sink->device, 0666);

    sink->inode = sink->device.inode;
    sink->devid = (sink->device.major << 16) | (sink->device.minor & 0xFFFF);
    sink->owned = true;
}


void usb_hid_sink_close(struct usb_hid_sink* sink) {

    if (!sink->inode)
        return;

    if (sink->owned)
        device_unlink(&sink->device);
    else
        vfs_inode_put(sink->inode);

    sink->inode = NULL;
    sink->owned = false;
}


void usb_hid_sink_write(struct usb_hid_sink* sink, event_t* ev) {

    if (unlikely(!sink->inode))
        return;

    ev->ev_devid = sink->devid;

    vfs_write(sink->inode, ev, 0, sizeof(*ev));
}



/**
 * @brief Drains an interface's interrupt queue into its handler, attaching one if it just appeared.
 * @param dev The device.
 */
static void usb_hid_poll(usbdev_t* dev) {

    usbhid_t* hid = HID_INST(dev);
    const u8* buf;

    if (!hid->handler) {

        const struct usb_hid_handler* h = usb_hid_handler(hid->protocol);

        if (h && (hid->ctx = h->attach(dev)))
            hid->handler = h;
    }

    while ((buf = dev->controller->poll_intr_queue(hid->queue))) {

        if (!hid->handler)
            continue;

        uint8_t report[USB_HID_REPORT_MAX] = {0};

        memcpy(report, buf, hid->size);

        hid->handler->report(hid->ctx, report);
    }

    if (hid->handler && hid->handler->tick)
        hid->handler->tick(hid->ctx);
}


/**
 * @brief Detaches the handler and tears the interrupt queue down.
 * @param dev The device going away.
 */
static void usb_hid_destroy(usbdev_t* dev) {

    usbhid_t* hid = HID_INST(dev);

    if (!hid)
        return;

    if (hid->handler)
        hid->handler->detach(hid->ctx);

    if (hid->queue)
        dev->controller->destroy_intr_queue(hid->ep, hid->queue);

    kfree(hid);
    dev->data = NULL;
}


/**
 * @brief Sends a class request with no data stage to an interface.
 * @param dev The device.
 * @param intf The interface number.
 * @param request The HID class request.
 * @param value The request's wValue.
 */
static void usb_hid_class_request(usbdev_t* dev, int intf, int request, int value) {

    dev_req_t dr;

    dr.bmRequestType = gen_bmRequestType(host_to_device, class_type, iface_recp);
    dr.bRequest      = request;
    dr.wValue        = value;
    dr.wIndex        = intf;
    dr.wLength       = 0;

    dev->controller->control(dev, OUT, sizeof(dr), &dr, 0, NULL);
}


/**
 * @brief Finds the first interface descriptor of the active configuration.
 * @param dev The device.
 * @return The interface, or NULL if the configuration has none.
 */
static interface_descriptor_t* usb_hid_interface(usbdev_t* dev) {

    configuration_descriptor_t* cd = dev->configuration;

    u8* ptr = (u8*)cd + cd->bLength;
    u8* end = (u8*)cd + cd->wTotalLength;

    for (; ptr + 2 <= end && ptr[0] && ptr + ptr[0] <= end; ptr += ptr[0]) {
        if (ptr[1] == DT_INTF && ptr[0] >= sizeof(interface_descriptor_t))
            return (interface_descriptor_t*)ptr;
    }

    return NULL;
}


/**
 * @brief Claims a boot-protocol keyboard or mouse and starts its interrupt queue.
 * @param dev The device the core enumerated as HID.
 */
static void usb_hid_init(usbdev_t* dev) {

    dev->data    = NULL;
    dev->destroy = usb_hid_destroy;

    interface_descriptor_t* intf = usb_hid_interface(dev);

    if (!intf || intf->bInterfaceSubClass != HID_SUBCLASS_BOOT) {
        usb_debug("usbhid: not a boot interface, ignoring it\n");
        usb_detach_device(dev->controller, dev->address);
        return;
    }

    if (intf->bInterfaceProtocol != USB_HID_PROTOCOL_KEYBOARD && intf->bInterfaceProtocol != USB_HID_PROTOCOL_MOUSE) {
        usb_detach_device(dev->controller, dev->address);
        return;
    }

    endpoint_t* ep = NULL;

    for (int i = 1; i < dev->num_endp; i++) {
        if (dev->endpoints[i].type == INTERRUPT && dev->endpoints[i].direction == IN) {
            ep = &dev->endpoints[i];
            break;
        }
    }

    if (!ep) {
        usb_detach_device(dev->controller, dev->address);
        return;
    }

    usbhid_t* hid = kcalloc(1, sizeof(usbhid_t), GFP_KERNEL);

    if (!hid) {
        usb_detach_device(dev->controller, dev->address);
        return;
    }

    hid->ep       = ep;
    hid->protocol = intf->bInterfaceProtocol;
    dev->data     = hid;

    usb_hid_class_request(dev, intf->bInterfaceNumber, HID_REQUEST_SET_PROTOCOL, HID_BOOT_PROTOCOL);
    usb_hid_class_request(dev, intf->bInterfaceNumber, HID_REQUEST_SET_IDLE, 0);

    hid->size  = MIN(MAX(ep->maxpacketsize, 1), USB_HID_REPORT_MAX);
    hid->queue = dev->controller->create_intr_queue(ep, hid->size, HID_QUEUE_DEPTH, 10);

    if (!hid->queue) {
        usb_detach_device(dev->controller, dev->address);
        return;
    }

    dev->poll = usb_hid_poll;

    kprintf("usb-hid: %s %04x:%04x attached\n", hid->protocol == USB_HID_PROTOCOL_KEYBOARD ? "keyboard" : "mouse", dev->descriptor->idVendor, dev->descriptor->idProduct);
}



void init(const char* args) {

    if (usb_register_class(USB_CLASS_HID, usb_hid_init) < 0)
        kprintf("usb-hid: ERROR! the HID class already has a driver\n");
}


void dnit(void) {
    usb_unregister_class(USB_CLASS_HID);
}
