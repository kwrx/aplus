#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <wc/wc.h>
#include <wc/wc_display.h>

#include <aplus/fb.h>
#include <sys/ioctl.h>


static wc_display_t* queue = NULL;



static int wc_display_destroy(struct wc_display* display) {

    assert(display);

    if (display->cr) {
        cairo_destroy(display->cr);
    }

    if (display->surface) {
        cairo_surface_destroy(display->surface);
    }

    if (display->fd >= 0) {
        close(display->fd);
    }


    if (queue == display) {

        queue = display->next;

    } else {

        struct wc_display* prev = queue;

        while (prev && prev->next != display) {
            prev = prev->next;
        }

        if (prev) {
            prev->next = display->next;
        }
    }

    free(display);

    return 0;
}


static int wc_display_create(const char* device, size_t offset_x, size_t offset_y) {

    assert(device);


    int fd = -1;

    cairo_t* cr              = NULL;
    cairo_surface_t* surface = NULL;


    wc_display_t* display = NULL;

    if (!(display = calloc(1, sizeof(struct wc_display)))) {
        goto fail;
    }

    if (access(device, F_OK) != 0) {
        goto fail;
    }

    if ((fd = open(device, O_RDWR)) < 0) {
        goto fail;
    }


    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

    if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0) {
        goto fail;
    }

    if (ioctl(fd, FBIOPUT_VSCREENINFO, &var) < 0) {
        goto fail;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) {
        goto fail;
    }


    switch (var.bits_per_pixel) {

        case 8:
            surface = cairo_image_surface_create_for_data((unsigned char*)fix.smem_start, CAIRO_FORMAT_A8, var.xres, var.yres, fix.line_length);
            break;

        case 16:
            surface = cairo_image_surface_create_for_data((unsigned char*)fix.smem_start, CAIRO_FORMAT_RGB16_565, var.xres, var.yres, fix.line_length);
            break;

        case 24:
            surface = cairo_image_surface_create_for_data((unsigned char*)fix.smem_start, CAIRO_FORMAT_RGB24, var.xres, var.yres, fix.line_length);
            break;

        case 32:
            surface = cairo_image_surface_create_for_data((unsigned char*)fix.smem_start, CAIRO_FORMAT_ARGB32, var.xres, var.yres, fix.line_length);
            break;

        default:
            errno = ENOTSUP;
            goto fail;
    }


    if (!surface) {
        goto fail;
    }

    if (!(cr = cairo_create(surface))) {
        goto fail;
    }


    (display)->fd       = fd;
    (display)->surface  = surface;
    (display)->cr       = cr;
    (display)->var      = var;
    (display)->fix      = fix;
    (display)->offset_x = 0;
    (display)->offset_y = 0;
    (display)->next     = queue;

    wc_ref_init(&(display)->ref, wc_display_destroy, display);

    queue = display;


    LOG("Created display device %s (%dx%d, %d bpp)\n", device, var.xres, var.yres, var.bits_per_pixel);

    return 0;


fail:

    if (surface) {
        cairo_surface_destroy(surface);
    }

    if (cr) {
        cairo_destroy(cr);
    }

    if (display) {
        free(display);
    }

    if (fd >= 0) {
        close(fd);
    }

    LOG("Failed to create display device %s\n", device);

    return -1;
}



int wc_display_initialize(void) {

    size_t offset_x = 0;
    size_t offset_y = 0;

    for (size_t i = 0; i < WC_DISPLAY_MAX; i++) {

        char device[32];
        sprintf(device, "/dev/fb%zu", i);

        if (access(device, R_OK) != 0)
            continue;

        if (wc_display_create(device, offset_x, offset_y) < 0)
            return -1;

        offset_x += queue->width;
    }


    LOG("display subsystem initialized\n");

    return 0;
}


struct wc_display* wc_display_at_position(uint16_t x, uint16_t y) {

    struct wc_display* display = queue;

    while (display) {

        if (x >= display->offset_x && x < display->offset_x + display->var.xres && y >= display->offset_y && y < display->offset_y + display->var.yres) {

            return wc_ref_inc(&(display)->ref), display;
        }

        display = display->next;
    }

    return NULL;
}


struct wc_display* wc_display_primary() {

    return wc_ref_inc(&(queue->ref)), queue;
}


struct wc_display* wc_display_next(struct wc_display* display) {

    if (!display)
        return NULL;

    if (display->next) {
        return wc_ref_inc(&(display->next->ref)), display->next;
    }

    return NULL;
}
