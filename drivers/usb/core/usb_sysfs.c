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
#include <stdio.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/sysfs.h>

#include <dev/usb/libpayload.h>
#include <dev/usb/usb.h>


#define USB_SYSFS_DEPTH_MAX 8


extern hci_t* usb_hcs;


/**
 * @brief Numbers a controller from 1 in registration order, as Linux numbers its USB buses.
 *
 * @param controller The controller.
 * @return Its bus number.
 */
static int usb_sysfs_busnum(hci_t* controller) {

    int n = 1;

    for (hci_t* hc = controller->next; hc; hc = hc->next)
        n++;

    return n;
}

/**
 * @brief Writes a device's Linux-style sysfs name, "<bus>-<port>[.<port>...]".
 *
 * @param dev The device, which must not be a root hub.
 * @param buf The output buffer.
 * @param size Its size.
 * @return 0 on success, -1 if the hub chain is broken or too deep.
 */
static int usb_sysfs_name(usbdev_t* dev, char* buf, size_t size) {

    int ports[USB_SYSFS_DEPTH_MAX];
    int depth = 0;

    for (usbdev_t* d = dev; d->address != 0; d = d->controller->devices[d->hub]) {

        if (depth == USB_SYSFS_DEPTH_MAX || d->hub < 0 || !d->controller->devices[d->hub])
            return -1;

        ports[depth++] = d->port;
    }

    if (depth == 0)
        return -1;

    int n = snprintf(buf, size, "%d-%d", usb_sysfs_busnum(dev->controller), ports[depth - 1]);

    for (int i = depth - 2; i >= 0 && n > 0 && (size_t)n < size; i--)
        n += snprintf(buf + n, size - n, ".%d", ports[i]);

    return (n > 0 && (size_t)n < size) ? 0 : -1;
}


/**
 * @brief Calls fn on every interface of a device's default settings.
 *
 * @param dev The device.
 * @param fn The callback.
 * @param name The device's sysfs name, handed through to fn.
 */
static void usb_sysfs_foreach_interface(usbdev_t* dev, void (*fn)(usbdev_t*, const interface_descriptor_t*, const char*), const char* name) {

    configuration_descriptor_t* cd = dev->configuration;

    if (!cd)
        return;

    u8* end = (u8*)cd + cd->wTotalLength;

    for (u8* p = (u8*)cd + sizeof(*cd); p + 2 <= end && p[0] && p + p[0] <= end; p += p[0]) {

        if (p[1] != DT_INTF || p[0] != sizeof(interface_descriptor_t))
            continue;

        const interface_descriptor_t* id = (const void*)p;

        if (id->bAlternateSetting != 0)
            continue;

        fn(dev, id, name);
    }
}


static void usb_sysfs_interface_name(usbdev_t* dev, const interface_descriptor_t* id, const char* name, char* buf, size_t size) {
    snprintf(buf, size, "%s:%d.%d", name, dev->configuration->bConfigurationValue, id->bInterfaceNumber);
}

static void usb_sysfs_interface_add(usbdev_t* dev, const interface_descriptor_t* id, const char* name) {

    const device_descriptor_t* dd = dev->descriptor;

    char ifname[CONFIG_MAXNAMLEN];
    usb_sysfs_interface_name(dev, id, name, ifname, sizeof(ifname));

    char uevent[384];
    snprintf(uevent, sizeof(uevent),
             "DEVTYPE=usb_interface\n"
             "PRODUCT=%x/%x/%x\n"
             "TYPE=%d/%d/%d\n"
             "INTERFACE=%d/%d/%d\n"
             "MODALIAS=usb:v%04Xp%04Xd%04Xdc%02Xdsc%02Xdp%02Xic%02Xisc%02Xip%02Xin%02X\n",
             dd->idVendor, dd->idProduct, dd->bcdDevice, dd->bDeviceClass, dd->bDeviceSubClass, dd->bDeviceProtocol, id->bInterfaceClass, id->bInterfaceSubClass, id->bInterfaceProtocol, dd->idVendor, dd->idProduct, dd->bcdDevice,
             dd->bDeviceClass, dd->bDeviceSubClass, dd->bDeviceProtocol, id->bInterfaceClass, id->bInterfaceSubClass, id->bInterfaceProtocol, id->bInterfaceNumber);

    sysfs_device_bus_add("usb", ifname, uevent);
}

static void usb_sysfs_interface_remove(usbdev_t* dev, const interface_descriptor_t* id, const char* name) {

    char ifname[CONFIG_MAXNAMLEN];
    usb_sysfs_interface_name(dev, id, name, ifname, sizeof(ifname));

    sysfs_device_bus_remove("usb", ifname);
}


/**
 * @brief Publishes an attached device and its interfaces under /sys/bus/usb/devices.
 *
 * @param dev The device.
 */
void usb_sysfs_add(usbdev_t* dev) {

    DEBUG_ASSERT(dev);

    const device_descriptor_t* dd = dev->descriptor;

    char name[CONFIG_MAXNAMLEN];

    if (!dd || usb_sysfs_name(dev, name, sizeof(name)) < 0)
        return;

    int busnum = usb_sysfs_busnum(dev->controller);

    char uevent[256];
    snprintf(uevent, sizeof(uevent),
             "PRODUCT=%x/%x/%x\n"
             "TYPE=%d/%d/%d\n"
             "BUSNUM=%03d\n"
             "DEVNUM=%03d\n"
             "DEVTYPE=usb_device\n",
             dd->idVendor, dd->idProduct, dd->bcdDevice, dd->bDeviceClass, dd->bDeviceSubClass, dd->bDeviceProtocol, busnum, dev->address);

    if (sysfs_device_bus_add("usb", name, uevent) < 0)
        return;

    usb_sysfs_foreach_interface(dev, usb_sysfs_interface_add, name);
}

/**
 * @brief Withdraws what usb_sysfs_add() published; called while the device and its hub chain still exist.
 *
 * @param dev The device.
 */
void usb_sysfs_remove(usbdev_t* dev) {

    DEBUG_ASSERT(dev);

    char name[CONFIG_MAXNAMLEN];

    if (usb_sysfs_name(dev, name, sizeof(name)) < 0)
        return;

    usb_sysfs_foreach_interface(dev, usb_sysfs_interface_remove, name);

    sysfs_device_bus_remove("usb", name);
}
