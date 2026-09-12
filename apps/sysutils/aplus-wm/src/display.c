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

    /* The only path that pushes pixels to the host on virtio-gpu. On the bochs and vmware
       adapters the driver has no wait_vsync hook and the ioctl returns success without
       doing anything, so it is safe to call unconditionally. */
    uint32_t crtc = 0;

    ioctl(display->fd, FBIO_WAITFORVSYNC, &crtc);
}
