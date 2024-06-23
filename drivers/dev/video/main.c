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
#include <sys/sysmacros.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/memory.h>
#include <aplus/hal.h>
#include <aplus/errno.h>

#include <dev/interface.h>
#include <dev/video.h>

#include <aplus/fb.h>



MODULE_NAME("dev/video");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


int video_getattr(device_t* device, struct stat* st) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(st);

    st->st_dev     = makedev(device->major, device->minor);
    st->st_ino     = device->inode->ino;
    st->st_mode    = S_IFCHR | 0644;
    st->st_nlink   = 1;
    st->st_uid     = 0;
    st->st_gid     = 0;
    st->st_rdev    = makedev(device->major, device->minor);
    st->st_size    = 0;
    st->st_blksize = 0;
    st->st_blocks  = 0;
    st->st_atime   = arch_timer_gettime();
    st->st_mtime   = arch_timer_gettime();
    st->st_ctime   = arch_timer_gettime();

    return 0;

}


int video_ioctl(device_t* device, int req, void * arg) {

    DEBUG_ASSERT(device);

    switch (req) {

        case FBIOGET_VSCREENINFO:

            uio_memcpy_s2u(arg, &device->vid.vs, sizeof(struct fb_var_screeninfo));
            break;


        case FBIOPUT_VSCREENINFO:
        
            uio_memcpy_u2s(&device->vid.vs, arg, sizeof(struct fb_var_screeninfo));

            if(likely(device->vid.update))
                device->vid.update(device);

            break;


        case FBIOGET_FSCREENINFO:

            uio_memcpy_s2u(arg, &device->vid.fs, sizeof(struct fb_fix_screeninfo));
            break;

        case FBIO_WAITFORVSYNC:

            if(likely(device->vid.wait_vsync))
                device->vid.wait_vsync(device);
            
            break;


        default:
            return errno = ENOSYS, -1;

    }

    return 0;
}


void video_init(device_t* device) {
    DEBUG_ASSERT(device);
}


void video_dnit(device_t* device) {
    DEBUG_ASSERT(device);
}


void init(const char* args) {
    (void) args;
}

void dnit(void) {

}