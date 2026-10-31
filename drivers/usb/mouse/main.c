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
#include <aplus/input.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <dev/char.h>
#include <dev/interface.h>
#include <dev/usb/hid.h>


MODULE_NAME("usb/mouse");
MODULE_DEPS("dev/interface,dev/char,usb/hid,input/ps2");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#define MOUSE_BUTTONS 5


struct usbmouse {
    uint8_t buttons;
};


static struct usb_hid_sink sink = {
    .device =
        {
            .type        = DEVICE_TYPE_CHAR,
            .name        = "mouse",
            .description = "USB mouse input device",
            .major       = 13,
            .minor       = 33,
            .status      = DEVICE_STATUS_UNKNOWN,
            .chr.io      = CHAR_IO_FBF,
        },
};



static void usbmouse_buttons(struct usbmouse* mouse, uint8_t buttons) {

    const uint8_t changed = (buttons ^ mouse->buttons) & ((1 << MOUSE_BUTTONS) - 1);

    for (size_t i = 0; i < MOUSE_BUTTONS; i++) {

        if (!(changed & (1 << i)))
            continue;

        event_t ev = {};

        ev.ev_type     = EV_KEY;
        ev.ev_key.vkey = BTN_MOUSE + i;
        ev.ev_key.down = !!(buttons & (1 << i));

        usb_hid_sink_write(&sink, &ev);
    }

    mouse->buttons = buttons;
}


static void usbmouse_report(void* ctx, const uint8_t* report) {

    usbmouse_buttons(ctx, report[0]);

    const int dx = (int8_t)report[1];
    const int dy = (int8_t)report[2];
    const int dz = (int8_t)report[3];

    if (!dx && !dy && !dz)
        return;

    event_t ev = {};

    ev.ev_type  = EV_REL;
    ev.ev_rel.x = (vaxis_t)dx;
    ev.ev_rel.y = (vaxis_t)-dy;
    ev.ev_rel.z = (vaxis_t)dz;

    usb_hid_sink_write(&sink, &ev);
}


static void* usbmouse_attach(usbdev_t* dev) {
    return kcalloc(1, sizeof(struct usbmouse), GFP_KERNEL);
}


static void usbmouse_detach(void* ctx) {

    usbmouse_buttons(ctx, 0);

    kfree(ctx);
}


static const struct usb_hid_handler handler = {
    .protocol = USB_HID_PROTOCOL_MOUSE,
    .attach   = usbmouse_attach,
    .report   = usbmouse_report,
    .tick     = NULL,
    .detach   = usbmouse_detach,
};



void init(const char* args) {

    usb_hid_sink_open(&sink, "/dev/mouse");

    if (usb_hid_register(&handler) < 0) {
#if DEBUG_LEVEL_ERROR
        kprintf("usb-mouse: ERROR! mice already have a handler\n");
#endif
    }
}


void dnit(void) {
    usb_hid_unregister(&handler);
    usb_hid_sink_close(&sink);
}
