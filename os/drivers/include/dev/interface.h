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

#ifndef _DEV_INTERFACE_H
#define _DEV_INTERFACE_H

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/ringbuffer.h>
#include <aplus/vfs.h>
#include <aplus/fb.h>
#include <stdint.h>



#define DEVICE_STATUS_UNKNOWN       0
#define DEVICE_STATUS_LOADING       1
#define DEVICE_STATUS_READY         2
#define DEVICE_STATUS_FAILED        3
#define DEVICE_STATUS_UNLOADING     4
#define DEVICE_STATUS_UNLOADED      5

#define DEVICE_TYPE_UNKNOWN         0
#define DEVICE_TYPE_CHAR            1
#define DEVICE_TYPE_BLOCK           2
#define DEVICE_TYPE_VIDEO           3



typedef struct device {
    int type;

    const char* name;
    const char* description;

    uint16_t vendorid;
    uint16_t deviceid;
    uint16_t intno;
    
    uintptr_t address;
    uintptr_t size;

    int status;
    
    spinlock_t lock;


    void (*init) (struct device*);
    void (*dnit) (struct device*);
    void (*reset) (struct device*);


    union {
        struct {
            union {
                struct {
                    void (*flush) (struct device*);
                };
                
                struct {
                    int (*write) (struct device*, const void*, size_t);
                    int (*read) (struct device*, void*, size_t);
                };
            };

            uint8_t io;
            ringbuffer_t buffer;
        } chr;

        struct {

            size_t blksize;
            size_t blkcount;

            struct {
                uint8_t c_data[4096];
                uint8_t c_cached;
                uint32_t c_blkno;
            } cache;

            int (*write) (struct device*, const void*, off_t, size_t);
            int (*read) (struct device*, void*, off_t, size_t);

        } blk;

        struct {
            struct fb_var_screeninfo vs;
            struct fb_fix_screeninfo fs;

            void (*update) (struct device*);
        } vid;
    };

    void* userdata;
} device_t;



void device_mkdev(device_t*, mode_t);
void device_unlink(device_t*);
void device_reset(device_t*);
void device_error(device_t*, const char*);

#endif