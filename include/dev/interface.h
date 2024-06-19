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

#ifndef _DEV_INTERFACE_H
#define _DEV_INTERFACE_H

#ifndef __ASSEMBLY__


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/vfs.h>
#include <aplus/network.h>
#include <aplus/fb.h>
#include <aplus/pty.h>

#include <aplus/utils/ringbuffer.h>

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
#define DEVICE_TYPE_NETWORK         4

#define DEVICE_MAXNAMELEN           32
#define DEVICE_MAXDESCLEN           256



typedef struct device {

    int type;
    int status;

    char name[DEVICE_MAXNAMELEN];
    char description[DEVICE_MAXDESCLEN];

    uint16_t major;
    uint16_t minor;

    uintptr_t iobase;
    uintptr_t mmiobase;

    
    spinlock_t lock;


    void (*init) (struct device*);
    void (*dnit) (struct device*);
    void (*reset) (struct device*);


    inode_t* inode;


    union {
        struct {
            union {
                struct {
                    void (*flush) (struct device*);
                };
                struct {
                    ssize_t (*write) (struct device*, const void*, size_t);
                    ssize_t (*read) (struct device*, void*, size_t);
                };
            };

            uint8_t io;
            ringbuffer_t buffer;

        } chr;

        struct {

            size_t blksize;
            size_t blkcount;
            size_t blkmax;
            size_t blkoff;

            struct {
                uint8_t c_data[8192];
                uint8_t c_cached;
                uint32_t c_blkno;
            } cache;

            ssize_t (*write) (struct device*, const void*, off_t, size_t);
            ssize_t (*read) (struct device*, void*, off_t, size_t);

        } blk;

        struct {
            
            struct fb_var_screeninfo vs;
            struct fb_fix_screeninfo fs;

            uintptr_t fb_base;
            uintptr_t fb_size;

            void (*update) (struct device*);
            void (*wait_vsync) (struct device*);

        } vid;

        struct {

            void (*low_level_init) (void*, uint8_t*, void*);
            int  (*low_level_startoutput) (void*);
            void (*low_level_output) (void*, void*, uint16_t);
            void (*low_level_endoutput) (void*, uint16_t);
            int  (*low_level_startinput) (void*);
            void (*low_level_input) (void*, void*, uint16_t);
            void (*low_level_endinput) (void*);
            void (*low_level_input_nomem) (void*, uint16_t);

            void* internals;
            uint8_t address[6];

            ip4_addr_t ip;
            ip4_addr_t nm;
            ip4_addr_t gw;

            struct netif interface;

        } net;

    };


    void* userdata;

} device_t;



__BEGIN_DECLS

void device_mkdev(device_t*, mode_t);
void device_unlink(device_t*);
void device_reset(device_t*);
void device_error(device_t*, const char*);

__END_DECLS


#endif
#endif