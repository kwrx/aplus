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
#include <stdlib.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/module.h>
#include <aplus/vfs.h>

#include <dev/char.h>
#include <dev/interface.h>



MODULE_NAME("char/urandom");
MODULE_DEPS("dev/interface,dev/char");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static ssize_t urandom_read(device_t*, void*, size_t);
static ssize_t urandom_write(device_t*, const void*, size_t);


device_t device = {

    .type = DEVICE_TYPE_CHAR,

    .name        = "urandom",
    .description = "Faster, less secure random number gen",

    .major = 1,
    .minor = 9,

    .status = DEVICE_STATUS_UNKNOWN,

    .init  = NULL,
    .dnit  = NULL,
    .reset = NULL,

    .chr.io    = CHAR_IO_NBF,
    .chr.write = &urandom_write,
    .chr.read  = &urandom_read,

};



static ssize_t urandom_read(device_t* device, void* buf, size_t size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);


    size_t i = 0;

    for (; i + 8 < size; i += 8)
        *(uint64_t*)(((uintptr_t)buf) + i) = arch_random() & 0xFFFFFFFFFFFFFFFF;

    for (; i + 4 < size; i += 4)
        *(uint32_t*)(((uintptr_t)buf) + i) = arch_random() & 0xFFFFFFFF;

    for (; i + 2 < size; i += 2)
        *(uint16_t*)(((uintptr_t)buf) + i) = arch_random() & 0xFFFF;

    for (; i < size; i += 1)
        *(uint8_t*)(((uintptr_t)buf) + i) = arch_random() & 0xFF;

    return size;
}

static ssize_t urandom_write(device_t* device, const void* buf, size_t size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);

    return size;
}


void init(const char* args) {
    device_mkdev(&device, 0666);
}


void dnit(void) {
    device_unlink(&device);
}
