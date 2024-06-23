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

#ifndef _DEV_VIDEO_H
#define _DEV_VIDEO_H

#ifndef __ASSEMBLY__


    #include <aplus.h>
    #include <aplus/debug.h>
    #include <aplus/syscall.h>
    #include <stdint.h>

    #include <dev/interface.h>


    #define DEVICE_VIDEO_MAX_DISPLAY   16
    #define DEVICE_VIDEO_MAX_RESOURCES 256

    #define DEVICE_VIDEO_DISPLAY_FLAGS_ENABLED 0x01
    #define DEVICE_VIDEO_DISPLAY_FLAGS_MIRROR  0x02

    #define DEVICE_VIDEO_RESOURCE_FLAGS_ENABLED 0x01
    #define DEVICE_VIDEO_RESOURCE_FLAGS_DIRTY   0x02
    #define DEVICE_VIDEO_RESOURCE_FLAGS_2D      0x04
    #define DEVICE_VIDEO_RESOURCE_FLAGS_3D      0x08



    #define VIDCTL_GET_DISPLAY_COUNT  0x00
    #define VIDCTL_GET_DISPLAY_INFO   0x01
    #define VIDCTL_GET_DISPLAY_MODE   0x02
    #define VIDCTL_SET_DISPLAY_MODE   0x03
    #define VIDCTL_RESOURCE_CREATE_2D 0x10
    #define VIDCTL_RESOURCE_CREATE_3D 0x11
    #define VIDCTL_RESOURCE_DESTROY   0x12
    #define VIDCTL_RESOURCE_GET_INFO  0x13
    #define VIDCTL_RESOURCE_RESIZE    0x14
    #define VIDCTL_RESOURCE_MOVE      0x15



__BEGIN_DECLS


typedef struct {

        uint16_t xoffset;
        uint16_t yoffset;
        uint16_t width;
        uint16_t height;
        uint16_t bpp;
        uint32_t pitch;
        uint32_t size;
        uint32_t flags;

        uintptr_t framebuffer;

} device_video_display_t;


typedef struct {

        uint16_t x;
        uint16_t y;
        uint16_t width;
        uint16_t height;
        uint16_t flags;
        uintptr_t buffer;

        spinlock_t lock;

} device_video_resource_t;


typedef struct {

        device_video_display_t root;
        device_video_display_t displays[DEVICE_VIDEO_MAX_DISPLAY];
        device_video_resource_t resources[DEVICE_VIDEO_MAX_RESOURCES];

        size_t display_count;
        size_t resources_count;

} device_video_context_t;



typedef struct {

        uint16_t width;
        uint16_t height;
        uint16_t bpp;

} device_video_display_mode_t;



int video_getattr(device_t*, struct stat*);
int video_ioctl(device_t*, int, void*);
void video_init(device_t*);
void video_dnit(device_t*);



int video_resource_create_2d(device_video_context_t*, uint16_t, uint16_t, uint16_t, uint16_t);
int video_resource_create_3d(device_video_context_t*, uint16_t, uint16_t, uint16_t, uint16_t);
int video_resource_destroy(device_video_context_t*, int);
int video_resource_lock(device_video_context_t*, int);
int video_resource_unlock(device_video_context_t*, int);



__END_DECLS

#endif
#endif
