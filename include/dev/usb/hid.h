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

#ifndef _DEV_USB_HID_H
#define _DEV_USB_HID_H

#include <aplus.h>
#include <aplus/events.h>
#include <aplus/vfs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dev/interface.h>
#include <dev/usb/usb.h>


#define USB_HID_PROTOCOL_KEYBOARD 1
#define USB_HID_PROTOCOL_MOUSE    2

#define USB_HID_REPORT_MAX 8


/** @brief Turns the boot reports of one kind of HID interface into input events. */
struct usb_hid_handler {

    /** @brief The boot protocol handled, USB_HID_PROTOCOL_KEYBOARD or USB_HID_PROTOCOL_MOUSE. */
    int protocol;

    /**
     * @brief Starts handling a device.
     * @param dev The device.
     * @return The handler's state for the device, or NULL to leave it unhandled.
     */
    void* (*attach)(usbdev_t* dev);

    /**
     * @brief Handles one report.
     * @param ctx The state attach() returned.
     * @param report The report, zero-padded to USB_HID_REPORT_MAX bytes.
     */
    void (*report)(void* ctx, const uint8_t* report);

    /**
     * @brief Runs on every poll of the device, reports or not.
     * @param ctx The state attach() returned.
     */
    void (*tick)(void* ctx);

    /**
     * @brief Stops handling a device that went away, releasing its state.
     * @param ctx The state attach() returned.
     */
    void (*detach)(void* ctx);
};


/** @brief An input node events are written to: one another driver already made, or one of ours. */
struct usb_hid_sink {
    device_t device;
    inode_t* inode;
    uint32_t devid;
    bool owned;
};


/**
 * @brief Gives the boot interfaces of a protocol to a handler, including those already attached.
 * @param handler The handler, which must outlive its registration.
 * @return 0 on success, -1 if the protocol is taken.
 */
int usb_hid_register(const struct usb_hid_handler* handler);

/**
 * @brief Stops giving new interfaces to a handler.
 * @param handler The handler.
 */
void usb_hid_unregister(const struct usb_hid_handler* handler);

/**
 * @brief Attaches a sink to its /dev node, reusing an existing one or creating sink->device.
 * @param sink The sink, whose device describes the node to create.
 * @param path The node's path.
 */
void usb_hid_sink_open(struct usb_hid_sink* sink, const char* path);

/**
 * @brief Releases a sink's node, unlinking it if the sink created it.
 * @param sink The sink.
 */
void usb_hid_sink_close(struct usb_hid_sink* sink);

/**
 * @brief Writes one event to a sink, filling in its device id.
 * @param sink The sink.
 * @param ev The event.
 */
void usb_hid_sink_write(struct usb_hid_sink* sink, event_t* ev);

#endif
