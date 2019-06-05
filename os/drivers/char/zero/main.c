/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sdi/device.h>
#include <sdi/chrdev.h>


MODULE_NAME("char/zero");
MODULE_DEPS("sdi");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static int zero_read(chrdev_t*, void*, size_t);


chrdev_t device = {
    .name = "zero",
    .description = "Read zeroes from device",

    .deviceid = 0,
    .vendorid = 0,
    .intno = 0,

    .status = DEVICE_STATUS_UNKNOWN,
    .io = CHRDEV_IO_NBF,

    .ops.init = NULL,
    .ops.dnit = NULL,
    .ops.reset = NULL,

    .ops.write = NULL,
    .ops.read = &zero_read,
};




static int zero_read(chrdev_t* device, void* buf, size_t size) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);

    memset(buf, 0, size);
}


void init(const char* args) {
    //sdi_device_add(DEVICE_TYPE_CHRDEV, &device, 0444);
}


void dnit(void) {
    //sdi_device_remove(&device);
}