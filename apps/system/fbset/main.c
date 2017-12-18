#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <aplus/base.h>
#include <aplus/fb.h>
#include <aplus/kd.h>
#include <aplus/sysconfig.h>


static void show_usage(int argc, char** argv) {
    printf(
        "Use: fbset [OPTIONS] [MODE]\n"
        "Show and modify frame buffer device settings\n\n"
        "   -n, --now                   Change the video mode immediately\n"
        "   -s, --show                  Display the video mode settings\n"
        "   -i, --info                  Display all available frame buffer information\n"
        "   -f, --fb <DEVICE>           DEVICE gives the frame buffer device node\n"
        "   -g, --geometry              Set all geometry parameters at once in order\n"
        "                               <xres> <yres> <vxres> <vyres> <depth>\n"
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



static int show_current_settings(char* dev) {
    int fb = open(dev, O_RDONLY);
    if(fb < 0) {
        perror(dev);
        return -1;
    }

    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

    ioctl(fb, FBIOGET_VSCREENINFO, &var);
    ioctl(fb, FBIOGET_FSCREENINFO, &fix);
    close(fb);

    
    fprintf(stdout, "\nmode \"%dx%d\"\n", var.xres, var.yres);
    fprintf(stdout, "\tgeometry %d %d %d %d %d\n", var.xres, var.yres, var.xres_virtual, var.yres_virtual, var.bits_per_pixel);
    fprintf(stdout, "\ttimings %d %d %d %d %d %d %d\n", var.pixclock, var.left_margin, var.right_margin, var.upper_margin, var.lower_margin, var.hsync_len, var.vsync_len);
    fprintf(stdout, "\trgba %d/%d %d/%d %d/%d %d/%d\n", var.red.length, var.red.offset, var.green.length, var.green.offset, var.blue.length, var.blue.offset, var.transp.length, var.transp.offset);
    fprintf(stdout, "endmode\n\n");

    return 0;
}

static int show_current_info(char* dev) {
    int fb = open(dev, O_RDONLY);
    if(fb < 0) {
        perror(dev);
        return -1;
    }

    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

    ioctl(fb, FBIOGET_VSCREENINFO, &var);
    ioctl(fb, FBIOGET_FSCREENINFO, &fix);
    close(fb);

    
    fprintf(stdout, "\nmode \"%dx%d\"\n", var.xres, var.yres);
    fprintf(stdout, "\tgeometry %d %d %d %d %d\n", var.xres, var.yres, var.xres_virtual, var.yres_virtual, var.bits_per_pixel);
    fprintf(stdout, "\ttimings %d %d %d %d %d %d %d\n", var.pixclock, var.left_margin, var.right_margin, var.upper_margin, var.lower_margin, var.hsync_len, var.vsync_len);
    fprintf(stdout, "\trgba %d/%d %d/%d %d/%d %d/%d\n", var.red.length, var.red.offset, var.green.length, var.green.offset, var.blue.length, var.blue.offset, var.transp.length, var.transp.offset);
    fprintf(stdout, "endmode\n\n");


    static char* fbtype[] = {
        "PACKED_PIXELS",
        "PLANES",
        "INTERLEAVED_PLANES",
        "TEXT",
        "VGA_PLANES",
        "FOURCC",
        NULL
    };

    static char* fbvisual[] = {
        "MONO01",
        "MONO10",
        "TRUECOLOR",
        "PSEUDOCOLOR",
        "DIRECTCOLOR",
        "STATIC_PSEUDOCOLOR",
        "FOURCC",
        NULL
    };


    fprintf(stdout, "Frame buffer device information:\n");
    fprintf(stdout, "\tName           : %s\n", fix.id);
    fprintf(stdout, "\tAddress        : %p\n", fix.smem_start);
    fprintf(stdout, "\tSize           : %d\n", fix.smem_len);
    fprintf(stdout, "\tType           : %s\n", fbtype[fix.type]);
    fprintf(stdout, "\tVisual         : %s\n", fbvisual[fix.visual]);
    fprintf(stdout, "\tXPanStep       : %d\n", fix.xpanstep);
    fprintf(stdout, "\tYPanStep       : %d\n", fix.ypanstep);
    fprintf(stdout, "\tYWrapStep:     : %d\n", fix.ywrapstep);
    fprintf(stdout, "\tLineLength     : %d\n", fix.line_length);
    fprintf(stdout, "\tAccelerator    : %s\n", fix.accel ? "Yes" : "No");

    return 0;
}


static int set_video_settings(char* dev, char** geometry) {
    if(getuid() != 0) {
        fprintf(stderr, "fbset: only superuser can edit frame buffer settings!\n");
        return -1;
    }

    int fb = open(dev, O_RDONLY);
    if(fb < 0) {
        perror(dev);
        return -1;
    }

    struct fb_var_screeninfo var;
    ioctl(fb, FBIOGET_VSCREENINFO, &var);

    var.xres = atoi(geometry[0]);
    var.yres = atoi(geometry[1]);
    var.xres_virtual = atoi(geometry[2]);
    var.yres_virtual = atoi(geometry[3]);
    var.bits_per_pixel = atoi(geometry[4]);

    ioctl(fb, FBIOPUT_VSCREENINFO, &var);
    close(fb);



    fb = open("/dev/console", O_RDONLY);
    if(fb < 0)
        return 0;

    ioctl(fb, KDSETMODE, KD_GRAPHICS);
    close(fb);

    return 0;
}

int main(int argc, char** argv) {
    static struct option long_options[] = {
        { "now", no_argument, NULL, 'n'},
        { "show", no_argument, NULL, 's'},
        { "info", no_argument, NULL, 'i'},
        { "fb", no_argument, NULL, 'f'},
        { "geometry", no_argument, NULL, 'g'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    

    
    char* dev = getenv("FRAMEBUFFER") 
                    ? (char*) getenv("FRAMEBUFFER")
                    : (char*) sysconfig("screen.device", SYSCONFIG_FORMAT_STRING, (uintptr_t) "/dev/fb0")
                    ;

    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "nsif:g:hv", long_options, &idx)) != -1) {
        switch(c) {
            case 'n':
                break;
            case 's':
                return show_current_settings(dev);
            case 'i':
                return show_current_info(dev);
            case 'f':
                dev = argv[optind];
                break;
            case 'g':
                return set_video_settings(dev, &argv[optind]);
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

    if(argc < optind + 1)
        ;//return set_video_mode(dev, argv[optind]);
    

    return show_current_info(dev);
}