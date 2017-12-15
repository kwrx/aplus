#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <aplus/base.h>
#include <aplus/sysconfig.h>
#include <aplus/fb.h>

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>



static void show_usage(int argc, char** argv) {
    printf(
        "Use: vga-terminal [OPTIONS]\n"
        "\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus sysutils) 0.1\n"
        "Copyright (c) 2016 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}



int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "", long_options, &idx)) != -1) {
        switch(c) {
            case 'v':
                show_version(argc, argv);
                break;
            case 'h':
            case '?':
                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }


    if(getuid() != 0) {
        fprintf(stderr, "vga-terminal: only superuser can run vga-terminal\n");
        return -1;
    }


    char* dev = (char*) sysconfig("screen.device", SYSCONFIG_FORMAT_STRING, (uintptr_t) "/dev/fb0");    


    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

    int fb = open(dev, O_RDONLY);
    if(fb < 0) {
        perror("vga-terminal: open framebuffer");
        return -1;
    }


    memset(&var, 0, sizeof(var));
    var.xres =
    var.xres_virtual = sysconfig("screen.width", SYSCONFIG_FORMAT_INT, 800);
    var.yres =
    var.yres_virtual = sysconfig("screen.heigth", SYSCONFIG_FORMAT_INT, 600);
    var.bits_per_pixel = sysconfig("screen.bpp", SYSCONFIG_FORMAT_INT, 32);
    var.activate = FB_ACTIVATE_NOW;

    ioctl(fb, FBIOPUT_VSCREENINFO, &var);
    ioctl(fb, FBIOGET_VSCREENINFO, &var);
    ioctl(fb, FBIOGET_FSCREENINFO, &fix);
    close(fb);




    cairo_format_t format;
    switch(var.bits_per_pixel) {
        case 16:
            format = CAIRO_FORMAT_RGB16_565;
            break;
        case 24:
            format = CAIRO_FORMAT_RGB24;
            break;
        case 32:
            format = CAIRO_FORMAT_ARGB32;
            break;
        default:
            fprintf(stderr, "vga-terminal: unsupported BitsPerPixel: %d\n", var.bits_per_pixel);
            return -1;
    }


    cairo_t* cx = cairo_create (
        cairo_image_surface_create_for_data (
            (unsigned char*) fix.smem_start,
            format,
            var.xres_virtual,
            var.yres_virtual,
            fix.line_length
        )
    );

    if(!cx) {
        fprintf(stderr, "vga-terminal: cairo_create() failed!");
        return -1;
    }


    cairo_save(cx);
    cairo_rectangle(cx, 0.0, 0.0, var.xres_virtual, var.yres_virtual);
    cairo_set_source_rgba(cx, 0.0, 0.0, 0.0, 1.0);
    cairo_fill(cx);
    cairo_restore(cx);


    int fd[2];
    if(pipe(fd) != 0) {
        perror("vga-terminal: pipe()");
        return -1;
    }

    if(fork() == 0) {

    }



    return 0;
}