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
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/module.h>

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


int video_ioctl(device_t* device, int req, void* arg) {

    DEBUG_ASSERT(device);

    switch (req) {

        case FBIOGET_VSCREENINFO:

            uio_memcpy_s2u(arg, &device->vid.vs, sizeof(struct fb_var_screeninfo));
            break;


        case FBIOPUT_VSCREENINFO:

            uio_memcpy_u2s(&device->vid.vs, arg, sizeof(struct fb_var_screeninfo));

            if (likely(device->vid.update))
                device->vid.update(device);

            break;


        case FBIOGET_FSCREENINFO:

            uio_memcpy_s2u(arg, &device->vid.fs, sizeof(struct fb_fix_screeninfo));
            break;

        case FBIO_WAITFORVSYNC:

            if (likely(device->vid.wait_vsync))
                device->vid.wait_vsync(device);

            break;


        case FBIO_FLUSH: {

            /* An adapter without a flush hook scans out of the framebuffer directly, so the
               pixels are already on screen and there is nothing to push. Succeeding here is
               what lets a compositor call this on every frame without first working out
               which kind of adapter it is looking at. */
            if (!device->vid.flush)
                break;

            struct fb_rect rect;

            uio_memcpy_u2s(&rect, arg, sizeof(rect));

            if (!rect.width || !rect.height)
                break;

            /* Clip against the mode rather than trusting the caller: these go on to become a
               rectangle the adapter reads out of a framebuffer sized by the mode. */
            if (rect.x >= device->vid.vs.xres || rect.y >= device->vid.vs.yres)
                break;

            if (rect.width > device->vid.vs.xres - rect.x)
                rect.width = device->vid.vs.xres - rect.x;

            if (rect.height > device->vid.vs.yres - rect.y)
                rect.height = device->vid.vs.yres - rect.y;

            device->vid.flush(device, rect.x, rect.y, rect.width, rect.height);

        } break;


        case FBIOGET_HWCINFO:

            if (!(device->vid.hwc.flags & FB_HWCINFO_HAS_CURSOR))
                return errno = ENOTSUP, -1;

            uio_memcpy_s2u(arg, &device->vid.hwc, sizeof(struct fb_hwcinfo));
            break;


        case FBIOPUT_HWCURSOR: {

            if (!device->vid.cursor_set)
                return errno = ENOTSUP, -1;


            struct fb_hwcursor cursor;

            uio_memcpy_u2s(&cursor, arg, sizeof(cursor));


            /* A cursor with no image is only meaningful as a request to hide the plane. */
            if (cursor.flags & FB_HWCURSOR_ENABLE) {

                if (!cursor.image || !cursor.width || !cursor.height)
                    return errno = EINVAL, -1;

                if (unlikely(!uio_check(cursor.image, R_OK)))
                    return errno = EFAULT, -1;

                if (cursor.width > device->vid.hwc.max_width || cursor.height > device->vid.hwc.max_height)
                    return errno = EINVAL, -1;

                if (cursor.hot_x >= cursor.width || cursor.hot_y >= cursor.height)
                    return errno = EINVAL, -1;
            }


            uint32_t* image = NULL;

            if (cursor.flags & FB_HWCURSOR_ENABLE) {

                size_t pixels = (size_t)cursor.width * (size_t)cursor.height;

                if (!(image = kcalloc(pixels, sizeof(uint32_t), GFP_KERNEL)))
                    return errno = ENOMEM, -1;

                uio_memcpy_u2s(image, cursor.image, pixels * sizeof(uint32_t));
            }


            int e = device->vid.cursor_set(device, &cursor, image);

            if (image)
                kfree(image);

            if (e < 0)
                return -1;

        } break;


        case FBIOPUT_HWCURSOR_POS: {

            if (!device->vid.cursor_move)
                return errno = ENOTSUP, -1;

            struct fb_hwcursor_pos pos;

            uio_memcpy_u2s(&pos, arg, sizeof(pos));

            if (device->vid.cursor_move(device, pos.x, pos.y) < 0)
                return -1;

        } break;


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
    (void)args;
}

void dnit(void) {
}
