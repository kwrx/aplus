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
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <wm.h>


/* Hand the pointer to the adapter as a cursor plane image.
 *
 * Cairo keeps ARGB32 premultiplied and the framebuffer interface asks for straight alpha, so
 * the colour channels are divided back out on the way. It matters for the antialiased edge of
 * the outline, which is the only part of this arrow that is neither fully opaque nor fully
 * transparent -- premultiplied pixels handed over as straight ones would leave that edge a
 * shade too dark.
 *
 * Failing here is not an error worth refusing to start over: it just means the pointer gets
 * drawn into the frame the old way.
 */

static bool wm_display_cursor_init(wm_display_t* display) {

    struct fb_hwcinfo hwc;

    if (ioctl(display->fd, FBIOGET_HWCINFO, &hwc) < 0) {
        return false;
    }

    if (!(hwc.flags & FB_HWCINFO_HAS_CURSOR)) {
        return false;
    }

    if (hwc.max_width < WM_CURSOR_IMAGE_WIDTH || hwc.max_height < WM_CURSOR_IMAGE_HEIGHT) {
        fprintf(stderr, "aplus-wm: hardware cursor is limited to %ux%u, too small for the pointer\n", hwc.max_width, hwc.max_height);
        return false;
    }


    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WM_CURSOR_IMAGE_WIDTH, WM_CURSOR_IMAGE_HEIGHT);

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return false;
    }


    cairo_t* cr = cairo_create(surface);

    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return false;
    }

    wm_cursor_paint(cr, WM_CURSOR_HOT_X, WM_CURSOR_HOT_Y);

    cairo_destroy(cr);
    cairo_surface_flush(surface);


    uint32_t* image = calloc(WM_CURSOR_IMAGE_WIDTH * WM_CURSOR_IMAGE_HEIGHT, sizeof(uint32_t));

    if (!image) {
        cairo_surface_destroy(surface);
        return false;
    }


    const unsigned char* data = cairo_image_surface_get_data(surface);
    const int stride          = cairo_image_surface_get_stride(surface);

    for (int y = 0; y < WM_CURSOR_IMAGE_HEIGHT; y++) {

        const uint32_t* row = (const uint32_t*)(data + (size_t)y * stride);

        for (int x = 0; x < WM_CURSOR_IMAGE_WIDTH; x++) {

            const uint32_t pixel = row[x];
            const uint32_t alpha = pixel >> 24;

            if (alpha == 0) {
                image[y * WM_CURSOR_IMAGE_WIDTH + x] = 0;
                continue;
            }

            if (alpha == 0xFF) {
                image[y * WM_CURSOR_IMAGE_WIDTH + x] = pixel;
                continue;
            }

            const uint32_t r = (((pixel >> 16) & 0xFF) * 0xFF + alpha / 2) / alpha;
            const uint32_t g = (((pixel >> 8) & 0xFF) * 0xFF + alpha / 2) / alpha;
            const uint32_t b = (((pixel >> 0) & 0xFF) * 0xFF + alpha / 2) / alpha;

            image[y * WM_CURSOR_IMAGE_WIDTH + x] = (alpha << 24) | ((r > 0xFF ? 0xFF : r) << 16) | ((g > 0xFF ? 0xFF : g) << 8) | (b > 0xFF ? 0xFF : b);
        }
    }

    cairo_surface_destroy(surface);


    struct fb_hwcursor cursor = {

        .flags  = FB_HWCURSOR_ENABLE,
        .width  = WM_CURSOR_IMAGE_WIDTH,
        .height = WM_CURSOR_IMAGE_HEIGHT,
        .hot_x  = WM_CURSOR_HOT_X,
        .hot_y  = WM_CURSOR_HOT_Y,
        .x      = wm.pointer.x,
        .y      = wm.pointer.y,
        .image  = image,
    };

    int e = ioctl(display->fd, FBIOPUT_HWCURSOR, &cursor);

    free(image);

    if (e < 0) {
        fprintf(stderr, "aplus-wm: ioctl(FBIOPUT_HWCURSOR) failed: %s\n", strerror(errno));
        return false;
    }


    fprintf(stderr, "aplus-wm: using the hardware cursor plane\n");

    return true;
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


    display->hwcursor = wm_display_cursor_init(display);

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
