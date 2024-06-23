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
#include <aplus/errno.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <stdint.h>
#include <string.h>

#include <dev/char.h>
#include <dev/interface.h>


MODULE_NAME("char/zero");
MODULE_DEPS("dev/interface,dev/char");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static ssize_t zero_read(device_t *, void *, size_t);
static ssize_t zero_write(device_t *, const void *, size_t);


device_t device = {

    .type = DEVICE_TYPE_CHAR,

    .name        = "zero",
    .description = "Null byte source",

    .major = 1,
    .minor = 5,

    .status = DEVICE_STATUS_UNKNOWN,

    .init  = NULL,
    .dnit  = NULL,
    .reset = NULL,

    .chr.io    = CHAR_IO_NBF,
    .chr.write = &zero_write,
    .chr.read  = &zero_read,

};



static ssize_t zero_read(device_t *device, void *buf, size_t size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);

    memset(buf, 0, size);
    return size;
}

static ssize_t zero_write(device_t *device, const void *buf, size_t size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);

    return size;
}


void init(const char *args) {
    device_mkdev(&device, 0666);
}


void dnit(void) {
    device_unlink(&device);
}
