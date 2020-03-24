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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/hal.h>
#include <aplus/errno.h>

#include <dev/interface.h>
#include <dev/char.h>




MODULE_NAME("char/urandom");
MODULE_DEPS("dev/interface,dev/char");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static ssize_t urandom_read(device_t*, void*, size_t);


device_t device = {

    .type = DEVICE_TYPE_CHAR,

    .name = "urandom",
    .description = "Faster, less secure random number gen",

    .major = 1,
    .minor = 9,

    .status = DEVICE_STATUS_UNKNOWN,

    .init =  NULL,
    .dnit =  NULL,
    .reset = NULL,

    .chr.io =    CHAR_IO_NBF,
    .chr.write = NULL,
    .chr.read =  &urandom_read,

};




static ssize_t urandom_read(device_t* device, void* buf, size_t size) {
   
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);


    srand((int) arch_timer_generic_getms());

    char* bc = (char*) buf;
    size_t i;
    for(i = 0; i < size; i++)
        *bc++ = rand() % 256;

    return size;
    
}


void init(const char* args) {
    device_mkdev(&device, 0666);
}


void dnit(void) {
    device_unlink(&device);
}