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


/**
 * @brief Puts an image on the adapter's cursor plane, at the position the pointer is at.
 *
 * @param display The display owning the plane.
 * @param image The cursor pixels, in straight alpha.
 * @param width The width of the image.
 * @param height The height of the image.
 * @param hot_x The hotspot within the image.
 * @param hot_y The hotspot within the image.
 * @return 0 on success, or -1 with errno set.
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


/**
 * @brief Decides whether the pointer lives on a plane of its own, by asking the adapter for one.
 *
 * @param display The display to ask.
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


int wm_display_open(wm_display_t* display, const char* device, const char* wallpaper) {

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


    if (!display->fix.smem_start || !display->var.xres || !display->var.yres) {
        fprintf(stderr, "aplus-wm: wrong framebuffer configuration\n");
        close(fd);
        return -1;
    }


    cairo_format_t format;

    switch (display->var.bits_per_pixel) {

        case 32:
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


    display->back = wm_surface_create(CAIRO_FORMAT_RGB24, display->width, display->height);

    if (!display->back) {
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


    display->background = wm_wallpaper_load(wallpaper, display->width, display->height);

    if (!display->background) {
        display->background = wm_wallpaper_gradient(display->height);
    }

    if (!display->background || cairo_pattern_status(display->background) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "aplus-wm: cannot create the desktop background\n");
        wm_display_close(display);
        return -1;
    }


    fprintf(stderr, "aplus-wm: %s is %dx%d at %u bpp, pitch %u\n", device, display->width, display->height, display->var.bits_per_pixel, display->fix.line_length);


    wm_display_cursor_init(display);

    return 0;
}


void wm_display_close(wm_display_t* display) {

    if (display->background) {
        cairo_pattern_destroy(display->background);
        display->background = NULL;
    }

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


/**
 * @brief Puts the composited rectangles on the screen and hands them to the adapter.
 *
 * An empty rectangle is dropped rather than passed on: the adapter skips the flush for one and
 * then makes the vsync that follows transfer the whole screen to catch up.
 *
 * @param display The display to put them on.
 * @param rects The rectangles that were repainted.
 * @param count How many of them there are.
 */
void wm_display_flush(wm_display_t* display, const wm_rect_t* rects, size_t count) {

    if (!count) {
        return;
    }


    cairo_surface_flush(display->back);


    cairo_save(display->cr_screen);

    cairo_set_fill_rule(display->cr_screen, CAIRO_FILL_RULE_WINDING);

    for (size_t i = 0; i < count; i++) {
        cairo_rectangle(display->cr_screen, rects[i].x, rects[i].y, rects[i].width, rects[i].height);
    }

    cairo_clip(display->cr_screen);

    cairo_set_operator(display->cr_screen, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(display->cr_screen, display->back, 0, 0);
    cairo_paint(display->cr_screen);

    cairo_restore(display->cr_screen);


    cairo_surface_flush(display->screen);


    for (size_t i = 0; i < count; i++) {

        if (rects[i].width <= 0 || rects[i].height <= 0) {
            continue;
        }

        struct fb_rect damage = {

            .x      = (uint32_t)rects[i].x,
            .y      = (uint32_t)rects[i].y,
            .width  = (uint32_t)rects[i].width,
            .height = (uint32_t)rects[i].height,
        };

        ioctl(display->fd, FBIO_FLUSH, &damage);
    }

    uint32_t crtc = 0;

    ioctl(display->fd, FBIO_WAITFORVSYNC, &crtc);
}
