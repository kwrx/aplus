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
#include <aplus/mm.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include <dev/interface.h>
#include <dev/video.h>

#include <aplus/fb.h>


MODULE_NAME("dev/video");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



__thread_safe
int video_ioctl(device_t* device, int req, void* arg) {
    DEBUG_ASSERT(device);


    switch (req) {
        case FBIOGET_VSCREENINFO:

            memcpy(arg, &device->vid.vs, sizeof(struct fb_var_screeninfo));
            break;

        case FBIOPUT_VSCREENINFO:
        
            memcpy(&device->vid.vs, arg, sizeof(struct fb_var_screeninfo));

            if(likely(device->vid.update))
                device->vid.update(device);

            break;

        case FBIOGET_FSCREENINFO:

            memcpy(arg, &device->vid.fs, sizeof(struct fb_fix_screeninfo));
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