#include "config.h"

static cairo_t* cr;
static cairo_surface_t* surface;
static fbdev_mode_t vm;


int surface_format = 0;
int surface_bpp = 0;


void init_display() {
    int fb = open("/dev/fb0", O_RDONLY);
    if(fb < 0) {
        perror("/dev/fb0");
        exit(1);
    }


    vm.width = sysconfig("screen.width", SYSCONFIG_FORMAT_INT, 800);
    vm.height = sysconfig("screen.height", SYSCONFIG_FORMAT_INT, 600);
    vm.bpp = sysconfig("screen.bpp", SYSCONFIG_FORMAT_INT, 32);
    vm.vx =
    vm.vy = 0;

    ioctl(fb, FBIOCTL_SETMODE, &vm);
    ioctl(fb, FBIOCTL_GETMODE, &vm);

    fprintf(stdout, "gnx: set video mode %dx%dx%d\n", vm.width, vm.height, vm.bpp);


    switch(vm.bpp) {
        case 16:
            surface_format = CAIRO_FORMAT_RGB16_565;
            break;
        case 24:
            surface_format = CAIRO_FORMAT_RGB24;
            break;
        case 32:
            surface_format = CAIRO_FORMAT_ARGB32;
            break;
        default:
            fprintf(stdout, "gnx: unsupported BitsPerPixel: %d\n", vm.bpp);
            exit(-1);
    }

    surface_bpp = vm.bpp;


    surface = cairo_image_surface_create_for_data (
        (unsigned char*) vm.lfbptr,
        surface_format,
        vm.width,
        vm.height,
        vm.width * (vm.bpp / 8)
    );

    if(!surface) {
        fprintf(stdout, "gnx: cairo_image_create_for_data(surface) failed!\n");
        exit(-1);
    }

    cr = cairo_create(surface);
    if(!cr) {
        fprintf(stdout, "gnx: cairo_create(cr) failed\n");
        exit(-1);
    }

    cairo_save(cr);
    cairo_rectangle(cr, 0.0, 0.0, vm.width, vm.height);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
    cairo_fill(cr);
    cairo_restore(cr);
}



void* th_display(void* arg) {
    fprintf(stdout, "gnx: initialized display controller: #%d\n", getpid());

    for(;;) {
        int dirty = 0;
        client_t* tmp;
        for(tmp = client_queue; tmp; tmp = tmp->next)
            dirty += tmp->data->dirty;


        if(unlikely(!dirty))
            goto done;


        for(tmp = client_queue; tmp; tmp = tmp->next) {
            tmp->data->dirty = 0;
#if 1
            cairo_save(cr);
            cairo_set_source_surface(cr, tmp->surface, tmp->data->x, tmp->data->y);
            cairo_paint(cr);
            cairo_restore(cr);
#endif
        }

done:       
        usleep(16666);
    }
}
