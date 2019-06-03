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

#ifndef _SDI_CHARDEV_H
#define _SDI_CHARDEV_H

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ringbuffer.h>
#include <stdint.h>
#include <stdio.h>

#define CHARDEV_IO_NBF      0   // No Buffering
#define CHARDEV_IO_LBF      1   // Line Buffered
#define CHARDEV_IO_FBF      2   // Full Buffered

typedef struct chardev {
    char* name;
    char* description;

    uint16_t vendorid;
    uint16_t deviceid;
    uint16_t intno;

    int status;
    int io;

    struct {
        void (*init) (struct chardev*);
        void (*dnit) (struct chardev*);
        void (*reset) (struct chardev*);

        union {
            struct {
                void (*flush) (struct chardev*);
            };

            struct {
                int (*write) (struct chardev*, void* buf, size_t);
                int (*read) (struct chardev*, void* buf, size_t);
            };
        };
    } ops;

    ringbuffer_t buffer;
    spinlock_t lock;
} chardev_t;

#endif