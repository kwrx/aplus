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

#ifndef _SDI_CHRDEV_H
#define _SDI_CHRDEV_H

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ringbuffer.h>
#include <stdint.h>
#include <stdio.h>

#define CHRDEV_IO_NBF      0   // No Buffering
#define CHRDEV_IO_LBF      1   // Line Buffered
#define CHRDEV_IO_FBF      2   // Full Buffered

typedef struct chrdev {
    char* name;
    char* description;

    uint16_t vendorid;
    uint16_t deviceid;
    uint16_t intno;

    int status;
    int io;

    struct {
        void (*init) (struct chrdev*);
        void (*dnit) (struct chrdev*);
        void (*reset) (struct chrdev*);

        union {
            struct {
                void (*flush) (struct chrdev*);
            };

            struct {
                int (*write) (struct chrdev*, const void* buf, size_t);
                int (*read) (struct chrdev*, void* buf, size_t);
            };
        };
    } ops;

    ringbuffer_t buffer;
    spinlock_t lock;
} chrdev_t;


void chrdev_add(chrdev_t* device, mode_t mode);
void chrdev_remove(chrdev_t* device);
int chrdev_write(chrdev_t* device, const void __user * buf, size_t size);
int chrdev_read(chrdev_t* device, void __user * buf, size_t size);
int chrdev_flush(chrdev_t* device);

#endif