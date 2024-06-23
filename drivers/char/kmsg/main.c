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
#include <aplus/hal.h>
#include <aplus/module.h>
#include <aplus/vfs.h>

#include <dev/char.h>
#include <dev/interface.h>



MODULE_NAME("char/kmsg");
MODULE_DEPS("dev/interface,dev/char");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static ssize_t kmsg_read(device_t *, void *, size_t);
static ssize_t kmsg_write(device_t *, const void *, size_t);


device_t device = {

    .type = DEVICE_TYPE_CHAR,

    .name        = "kmsg",
    .description = "Writes to this come out as printk's, reads "
                   "export the buffered printk records.",

    .major = 1,
    .minor = 11,

    .status = DEVICE_STATUS_UNKNOWN,

    .init  = NULL,
    .dnit  = NULL,
    .reset = NULL,

    .chr.io    = CHAR_IO_NBF,
    .chr.write = &kmsg_write,
    .chr.read  = &kmsg_read,

};



static ssize_t kmsg_read(device_t *device, void *buf, size_t size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);

    return 0;
}


static ssize_t kmsg_write(device_t *device, const void *buf, size_t size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);


    kprintf_pause();

    arch_debug_putc('\e');
    arch_debug_putc('[');
    arch_debug_putc('1');
    arch_debug_putc(';');
    arch_debug_putc('3');
    arch_debug_putc('7');
    arch_debug_putc('m');

    size_t i;
    for (i = 0; i < size; i++)
        arch_debug_putc(((char *)buf)[i]);

    arch_debug_putc('\e');
    arch_debug_putc('[');
    arch_debug_putc('0');
    arch_debug_putc('m');

    kprintf_resume();


    return size;
}


void init(const char *args) {
    device_mkdev(&device, 0666);
}


void dnit(void) {
    device_unlink(&device);
}
