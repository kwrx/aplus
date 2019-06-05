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

#ifndef _SDI_DEVICE_H
#define _SDI_DEVICE_H

#include <aplus.h>
#include <aplus/debug.h>
#include <stdint.h>

#include <sdi/chrdev.h>

#define DEVICE_MAGIC                0xFFDE71C3
#define DEVICE_MAX                  64


#define DEVICE_STATUS_UNKNOWN       0
#define DEVICE_STATUS_LOADING       1
#define DEVICE_STATUS_READY         2
#define DEVICE_STATUS_FAILED        3
#define DEVICE_STATUS_UNLOADING     4
#define DEVICE_STATUS_UNLOADED      5

#define DEVICE_TYPE_UNKNOWN         0
#define DEVICE_TYPE_CHRDEV          1


typedef struct {
    int magic;
    int type;

    union {
        chrdev_t chrdev;
    };
} device_t;

#endif