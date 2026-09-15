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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <wm.h>


/* Put an image on the adapter's cursor plane, at the position the pointer is at.
 *
 * The pixels go over untouched: the theme decodes to straight alpha, which is exactly how the
 * framebuffer interface specifies a cursor image. It is cairo that wants them premultiplied,
 * and that is a copy the cursor code keeps separately for the software path.
 */

int wm_display_cursor_image(wm_display_t* display, const uint32_t* image, int width, int height, int hot_x, int hot_y) {

    struct fb_hwcursor cursor = {

        .flags  = FB_HWCURSOR_ENABLE,
        .width  = (uint32_t)width,
        .height = (uint32_t)height,
        .hot_x  = (uint32_t)hot_x,
        .hot_y  = (uint32_t)hot_y,
        .x      = wm.pointer.x,
        .y      = wm.pointer.y,
        .image  = image,
    };

    if (ioctl(display->fd, FBIOPUT_HWCURSOR, &cursor) < 0) {
        fprintf(stderr, "aplus-wm: ioctl(FBIOPUT_HWCURSOR) failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}


/* Decide whether the pointer lives on a plane of its own, by asking the adapter for one and
 * putting the current shape on it.
 *
 * The flag has to be set before that first upload rather than after it: it is what tells the
 * cursor code to keep a decoded image in the straight-alpha form the plane wants, instead of
 * releasing it once the premultiplied copy is built.
 *
 * Failing here is not an error worth refusing to start over: it just means the pointer gets
 * drawn into the frame the old way.
 */

static void wm_display_cursor_init(wm_display_t* display) {

    struct fb_hwcinfo hwc;

    if (ioctl(display->fd, FBIOGET_HWCINFO, &hwc) < 0) {
        return;
    }

    if (!(hwc.flags & FB_HWCINFO_HAS_CURSOR)) {
        return;
    }


    display->hwcursor = true;

    if (wm_cursor_upload() < 0) {

        display->hwcursor = false;

        return;
    }


    fprintf(stderr, "aplus-wm: using the hardware cursor plane\n");
}


void wm_display_cursor_move(wm_display_t* display, int x, int y) {

    if (!display->hwcursor) {
        return;
    }

    struct fb_hwcursor_pos pos = {.x = x, .y = y};

    ioctl(display->fd, FBIOPUT_HWCURSOR_POS, &pos);
}


int wm_display_open(wm_display_t* display, const char* device) {

    memset(display, 0, sizeof(*display));

    display->fd = -1;


    int fd;

    if ((fd = open(device, O_RDWR)) < 0) {
        fprintf(stderr, "aplus-wm: open() failed: cannot open %s: %s\n", device, strerror(errno));
        return -1;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &display->var) < 0) {
        fprintf(stderr, "aplus-wm: ioctl(FBIOGET_VSCREENINFO) failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &display->fix) < 0) {
        fprintf(stderr, "aplus-wm: ioctl(FBIOGET_FSCREENINFO) failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }


    /* smem_start is a physical address that the video driver identity-mapped with the user
       and global bits set, so it is directly dereferenceable here and stays valid across
       fork(). There is no mmap to do: mmap(MAP_SHARED) is ENOTSUP and /dev/fb0 rejects
       read() and write() outright. */
    if (!display->fix.smem_start || !display->var.xres || !display->var.yres) {
        fprintf(stderr, "aplus-wm: wrong framebuffer configuration\n");
        close(fd);
        return -1;
    }


    cairo_format_t format;

    switch (display->var.bits_per_pixel) {

        case 32:
            /* RGB24 rather than ARGB32: the framebuffer is opaque, and treating the top
               byte as alpha would ask cairo for premultiplied data nobody produces. */
            format = CAIRO_FORMAT_RGB24;
            break;

        case 16:
            format = CAIRO_FORMAT_RGB16_565;
            break;

        default:
            fprintf(stderr, "aplus-wm: unsupported framebuffer depth: %u bpp\n", display->var.bits_per_pixel);
            close(fd);
            return -1;
    }


    display->fd     = fd;
    display->width  = (int)display->var.xres;
    display->height = (int)display->var.yres;


    display->screen = cairo_image_surface_create_for_data((unsigned char*)display->fix.smem_start, format, display->width, display->height, (int)display->fix.line_length);

    if (cairo_surface_status(display->screen) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "aplus-wm: cannot wrap the framebuffer: %s\n", cairo_status_to_string(cairo_surface_status(display->screen)));
        wm_display_close(display);
        return -1;
    }


    /* Everything is composited off-screen and blitted once per frame. Painting straight
       into the framebuffer would show the background, then each window, as separate
       flashes. */
    display->back = cairo_image_surface_create(CAIRO_FORMAT_RGB24, display->width, display->height);

    if (cairo_surface_status(display->back) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "aplus-wm: cannot allocate the back buffer\n");
        wm_display_close(display);
        return -1;
    }


    display->cr        = cairo_create(display->back);
    display->cr_screen = cairo_create(display->screen);

    if (cairo_status(display->cr) != CAIRO_STATUS_SUCCESS || cairo_status(display->cr_screen) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "aplus-wm: cannot create a drawing context\n");
        wm_display_close(display);
        return -1;
    }


    fprintf(stderr, "aplus-wm: %s is %dx%d at %u bpp, pitch %u\n", device, display->width, display->height, display->var.bits_per_pixel, display->fix.line_length);


    wm_display_cursor_init(display);

    return 0;
}


void wm_display_close(wm_display_t* display) {

    if (display->cr_screen) {
        cairo_destroy(display->cr_screen);
        display->cr_screen = NULL;
    }

    if (display->cr) {
        cairo_destroy(display->cr);
        display->cr = NULL;
    }

    if (display->back) {
        cairo_surface_destroy(display->back);
        display->back = NULL;
    }

    if (display->screen) {
        cairo_surface_destroy(display->screen);
        display->screen = NULL;
    }

    if (display->fd >= 0) {
        close(display->fd);
        display->fd = -1;
    }
}


void wm_display_flush(wm_display_t* display, const wm_rect_t* rect) {

    cairo_surface_flush(display->back);


    cairo_save(display->cr_screen);

    cairo_rectangle(display->cr_screen, rect->x, rect->y, rect->width, rect->height);
    cairo_clip(display->cr_screen);

    cairo_set_operator(display->cr_screen, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(display->cr_screen, display->back, 0, 0);
    cairo_paint(display->cr_screen);

    cairo_restore(display->cr_screen);


    cairo_surface_flush(display->screen);


    /* The only path that pushes pixels to the host on virtio-gpu, and the reason the damaged
       rectangle is carried all the way down here rather than being used just to clip the
       repaint: the transfer that FBIO_FLUSH triggers is the host reading guest memory, and at
       this mode a whole screen of it is three and a half megabytes. Most frames damage a
       character cell.

       On the bochs and vmware adapters the driver has no flush hook and the ioctl succeeds
       without doing anything, because there the framebuffer is the scanout and the pixels are
       already there. Both calls are safe to make unconditionally. */
    struct fb_rect damage = {

        .x      = (uint32_t)rect->x,
        .y      = (uint32_t)rect->y,
        .width  = (uint32_t)rect->width,
        .height = (uint32_t)rect->height,
    };

    ioctl(display->fd, FBIO_FLUSH, &damage);

    uint32_t crtc = 0;

    ioctl(display->fd, FBIO_WAITFORVSYNC, &crtc);
}
