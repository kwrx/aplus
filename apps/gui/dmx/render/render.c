#include "../dmx.h"

#include <aplus/fbdev.h>
#include <aplus/sysconfig.h>
#include <sys/times.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <ft2build.h>
#include FT_FREETYPE_H

int init_render(dmx_t* dmx) {
    TRACE("Initializing Rendering\n");

    int fb = open("/dev/fb0", O_RDONLY);
    if(fb < 0) {
        TRACE("/dev/fb0: could not open\n");
        return -1;
    }

    fbdev_mode_t vm;
    vm.width = sysconfig("screen.width", SYSCONFIG_FORMAT_INT, 800);
    vm.height = sysconfig("screen.height", SYSCONFIG_FORMAT_INT, 600);
    vm.bpp = sysconfig("screen.bpp", SYSCONFIG_FORMAT_INT, 32);
    vm.vx = 0;
    vm.vy = 0;

    ioctl(fb, FBIOCTL_SETMODE, &vm);
    ioctl(fb, FBIOCTL_GETMODE, &vm);


    switch(vm.bpp) {
        case 16:
            dmx->format = CAIRO_FORMAT_RGB16_565;
            break;
        case 24:
            dmx->format = CAIRO_FORMAT_RGB24;
            break;
        case 32:
            dmx->format = CAIRO_FORMAT_ARGB32;
            break;
        default:
            TRACE("Unsupported BitsPerPixel: %d\n", vm.bpp);
            return -1;
    }

    dmx->width = vm.width;
    dmx->height = vm.height;
    dmx->bpp = vm.bpp;
    dmx->stride = vm.width * (vm.bpp / 8);

    TRACE("Set VideoMode: %dx%dx%d\n", vm.width, vm.height, vm.bpp);


    dmx->frontbuffer = cairo_create (
        cairo_image_surface_create_for_data (
            (unsigned char*) vm.lfbptr,
            dmx->format,
            dmx->width,
            dmx->height,
            dmx->stride
        )
    );

    if(!dmx->frontbuffer) {
        TRACE("cairo_create() failed for dmx->frontbuffer\n");
        return -1;
    }


    dmx->backbuffer = cairo_create (
        cairo_image_surface_create (
            dmx->format,
            dmx->width,
            dmx->height
        )
    );

    if(!dmx->frontbuffer) {
        TRACE("cairo_create() failed for dmx->backbuffer\n");
        return -1;
    }


    cairo_save(dmx->backbuffer);
    cairo_rectangle(dmx->backbuffer, 0.0, 0.0, dmx->width, dmx->height);
    cairo_set_source_rgba(dmx->backbuffer, 0.0, 0.0, 0.0, 1.0);
    cairo_fill(dmx->backbuffer);
    cairo_restore(dmx->backbuffer);


    cairo_set_antialias(dmx->frontbuffer, CAIRO_ANTIALIAS_NONE);


    TRACE("Done!\n");
    return 0;
}



void* th_render(void* arg) {
    TRACE("Running\n");
    dmx_t* dmx = (dmx_t*) arg;

    for(;;) {
        list_each(dmx->dirtyrects, r) {
            cairo_save(dmx->backbuffer);
            cairo_rectangle(dmx->backbuffer, r->x, r->y, r->w, r->h);
            cairo_clip(dmx->backbuffer);
            cairo_new_path(dmx->backbuffer);
            
            list_each(dmx->clients, w)
                dmx_blit_view(dmx, w);

            cairo_restore(dmx->backbuffer);
            free(r);
        }

        list_clear(dmx->dirtyrects);

        if(!dmx->redraw)
            goto done;


        cairo_save(dmx->frontbuffer);
        cairo_set_operator(dmx->frontbuffer, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(dmx->frontbuffer, cairo_get_target(dmx->backbuffer), 0, 0);
        cairo_paint(dmx->frontbuffer);
        cairo_restore(dmx->frontbuffer);

        cairo_save(dmx->frontbuffer);
        cairo_set_source_surface(dmx->frontbuffer, dmx->cursors[dmx->cursor_index], dmx->cursor_x, dmx->cursor_y);
        cairo_paint(dmx->frontbuffer);
        cairo_restore(dmx->frontbuffer);


        dmx->redraw = 0;

done:
        usleep(16666);
        (void) 0;
    }
}