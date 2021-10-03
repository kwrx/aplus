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
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <string.h>

#include <dev/interface.h>
#include <dev/char.h>


MODULE_NAME("char/null");
MODULE_DEPS("dev/interface,dev/char");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



device_t device = {

    .type = DEVICE_TYPE_CHAR,

    .name = "null",
    .description = "Null device",

    .major = 1,
    .minor = 3,

    .status = DEVICE_STATUS_UNKNOWN,

    .init =  NULL,
    .dnit =  NULL,
    .reset = NULL,

    .chr.io =    CHAR_IO_NBF,
    .chr.write = NULL,
    .chr.read =  NULL,

};


void init(const char* args) {
    device_mkdev(&device, 0666);
}


void dnit(void) {
    device_unlink(&device);
}