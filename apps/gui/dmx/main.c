#include "dmx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>



static void show_usage(int argc, char** argv) {
    printf(
        "Use: dmx [OPTIONS...]\n"
        "Desktop Manager Server\n\n"
        "   -k, --kill                  kill all running server\n"
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


static int init_server(dmx_t* dmx) {    
    if(access(DMX_PIPE, F_OK) == 0) {
        fprintf(stderr, "dmx: server already running\n");
        return -1;
    }

    if(mkfifo(DMX_PIPE, 0666) != 0) {
        perror("dmx: " DMX_PIPE);
        return -1;
    }


    dmx->fd = open(DMX_PIPE, O_RDWR);
    if(dmx->fd < 0) {
        perror("dmx: " DMX_PIPE);
        return -1;
    }

    return 0;
}

static int init_screen(dmx_t* dmx) {
    char* dev = (char*) sysconfig("screen.device", "/dev/fb0");

    int fb = open(dev, O_RDONLY);
    if(fb < 0) {
        fprintf(stderr, "dmx: %s: could not open\n", dev);
        return -1;
    }

    struct fb_var_screeninfo var;
    memset(&var, 0, sizeof(var));

    var.xres = 
    var.xres_virtual = (int) sysconfig("screen.width", 800);
    var.yres = 
    var.yres_virtual = (int) sysconfig("screen.height", 600);
    var.bits_per_pixel = (int) sysconfig("screen.bpp", 32);
    var.activate = FB_ACTIVATE_NOW;

    ioctl(fb, FBIOPUT_VSCREENINFO, &var);
    ioctl(fb, FBIOGET_VSCREENINFO, &var);
    
    struct fb_fix_screeninfo fix;
    ioctl(fb, FBIOGET_FSCREENINFO, &fix);
    close(fb);

    switch(var.bits_per_pixel) {
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
            fprintf(stderr, "dmx: unsupported BitsPerPixel: %d\n", var.bits_per_pixel);
            return -1;
    }

    dmx->width = var.xres_virtual;
    dmx->height = var.yres_virtual;
    dmx->bpp = var.bits_per_pixel;
    dmx->stride = fix.line_length;


    dmx->frontbuffer = cairo_create (
        cairo_image_surface_create_for_data (
            (unsigned char*) fix.smem_start,
            dmx->format,
            dmx->width,
            dmx->height,
            dmx->stride
        )
    );

    if(!dmx->frontbuffer) {
        fprintf(stderr, "dmx: cairo_create() failed for dmx->frontbuffer\n");
        return -1;
    }


    dmx->backbuffer = cairo_create (
        cairo_image_surface_create (
            dmx->format,
            dmx->width,
            dmx->height
        )
    );

    if(!dmx->backbuffer) {
        fprintf(stderr, "dmx: cairo_create() failed for dmx->backbuffer\n");
        return -1;
    }


    cairo_save(dmx->backbuffer);
    cairo_rectangle(dmx->backbuffer, 0.0, 0.0, dmx->width, dmx->height);
    cairo_set_source_rgba(dmx->backbuffer, 0.0, 0.0, 0.0, 1.0);
    cairo_fill(dmx->backbuffer);
    cairo_restore(dmx->backbuffer);
    cairo_set_antialias(dmx->frontbuffer, CAIRO_ANTIALIAS_NONE);

    return 0;
}

static int init_fonts(dmx_t* dmx) {
    FILE* fp = fopen("/etc/fonts/fonts.conf", "r");
    if(!fp) {
        perror("dmx: /etc/fonts/fonts.conf");
        return -1;
    }

    char buf[BUFSIZ];
    while(fgets(buf, BUFSIZ, fp)) {
        if(!strchr(buf, ':'))
            continue;

        if(strchr(buf, '\n'))
            strchr(buf, '\n') [0] = '\0';

        char* p = strchr(buf, ':');
        *p++ = '\0';

        dmx_font_t* ft = (dmx_font_t*) calloc(1, sizeof(dmx_font_t));
        strcpy(ft->family, p);
        strcpy(ft->path, buf);

        list_push(dmx->fonts, ft);
    }

    fclose(fp);


    FT_Init_FreeType(&dmx->ft_library);


    int __obtain(dmx_t* dmx, FT_Face* face, char* family, char* subfamily) {
        char buf[BUFSIZ];
        memset(buf, 0, BUFSIZ);
        sprintf(buf, "%s %s", family, subfamily);


        dmx_font_t* ft;
        list_each(dmx->fonts, v) {
            if(strcmp(v->family, buf) != 0)
                continue;

            ft = v;
            break;
        }

        if(!ft) {
            fprintf(stderr, "dmx: %s: font not found!\n", buf);
            return -1;
        }
        
        FILE* fp = fopen(ft->path, "rb");
        if(!fp) {
            perror(ft->path);
            return -1;
        }

        fseek(fp, 0, SEEK_END);
        ft->cachesize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        ft->cache = (void*) malloc(ft->cachesize);
        if(!ft->cache) {
            fclose(fp);
            perror(buf);
            return -1;
        }

        fread(ft->cache, 1, ft->cachesize, fp);
        fclose(fp);
    
        if(FT_New_Memory_Face(dmx->ft_library, ft->cache, ft->cachesize, 0, face) != 0) {
            fprintf(stderr, "dmx: %s: FT_New_Memory_Face() failed!\n", buf);
            return -1;
        }

        return 0;
    }

    #define _(x, y, z)                                                                                          \
        if(__obtain(dmx, &dmx->ft_cache[x], (char*) sysconfig(y, 0), z) != 0)                                   \
            return -1;



    /* Condensed */
    _(DMX_FONT_TYPE_CONDENSED   | DMX_FONT_WEIGHT_REGULAR   | DMX_FONT_STYLE_NORMAL, "ui.font.condensed",   "Regular");

    /* Regular */
    _(DMX_FONT_TYPE_REGULAR     | DMX_FONT_WEIGHT_REGULAR   | DMX_FONT_STYLE_NORMAL, "ui.font.regular",     "Regular");
    _(DMX_FONT_TYPE_REGULAR     | DMX_FONT_WEIGHT_LIGHT     | DMX_FONT_STYLE_NORMAL, "ui.font.regular",     "Light");
    _(DMX_FONT_TYPE_REGULAR     | DMX_FONT_WEIGHT_MEDIUM    | DMX_FONT_STYLE_NORMAL, "ui.font.regular",     "Medium");
    _(DMX_FONT_TYPE_REGULAR     | DMX_FONT_WEIGHT_MEDIUM    | DMX_FONT_STYLE_NORMAL, "ui.font.regular",     "Bold");
    _(DMX_FONT_TYPE_REGULAR     | DMX_FONT_WEIGHT_REGULAR   | DMX_FONT_STYLE_ITALIC, "ui.font.regular",     "Italic");
    _(DMX_FONT_TYPE_REGULAR     | DMX_FONT_WEIGHT_LIGHT     | DMX_FONT_STYLE_ITALIC, "ui.font.regular",     "Light Italic");
    _(DMX_FONT_TYPE_REGULAR     | DMX_FONT_WEIGHT_MEDIUM    | DMX_FONT_STYLE_ITALIC, "ui.font.regular",     "Medium Italic");
    _(DMX_FONT_TYPE_REGULAR     | DMX_FONT_WEIGHT_MEDIUM    | DMX_FONT_STYLE_ITALIC, "ui.font.regular",     "Bold Italic");

    /* Monospace */
    _(DMX_FONT_TYPE_MONOSPACE   | DMX_FONT_WEIGHT_REGULAR   | DMX_FONT_STYLE_NORMAL, "ui.font.monospace",   "Regular");
    _(DMX_FONT_TYPE_MONOSPACE   | DMX_FONT_WEIGHT_MEDIUM    | DMX_FONT_STYLE_NORMAL, "ui.font.monospace",   "Bold");
    _(DMX_FONT_TYPE_MONOSPACE   | DMX_FONT_WEIGHT_REGULAR   | DMX_FONT_STYLE_ITALIC, "ui.font.monospace",   "Italic");
    _(DMX_FONT_TYPE_MONOSPACE   | DMX_FONT_WEIGHT_MEDIUM    | DMX_FONT_STYLE_ITALIC, "ui.font.monospace",   "Bold Italic");

    return 0;
}

static int init_input(dmx_t* dmx) {
    #define lc(p, i) {                                                              \
        dmx->cursors[i] = cairo_image_surface_create_from_webp(p);                  \
        if(!dmx->cursors[i]) {                                                      \
            switch(cairo_surface_status(dmx->cursors[i])) {                         \
                case CAIRO_STATUS_NO_MEMORY:                                        \
                    fprintf(stderr, "dmx: %s: CAIRO_STATUS_NO_MEMORY\n", p);        \
                    break;                                                          \
                case CAIRO_STATUS_FILE_NOT_FOUND:                                   \
                    fprintf(stderr, "dmx: %s: CAIRO_STATUS_FILE_NOT_FOUND\n", p);   \
                    break;                                                          \
                case CAIRO_STATUS_READ_ERROR:                                       \
                    fprintf(stderr, "dmx: %s: CAIRO_STATUS_READ_ERROR\n", p);       \
                    break;                                                          \
            }                                                                       \
                                                                                    \
            return -1;                                                              \
        }                                                                           \
    }

    
    lc(PATH_CURSORS "/arrow.webp",      DMX_CURSOR_ARROW);
    lc(PATH_CURSORS "/cross.webp",      DMX_CURSOR_CROSS);
    lc(PATH_CURSORS "/forbidden.webp",  DMX_CURSOR_FORBIDDEN);
    lc(PATH_CURSORS "/hand.webp",       DMX_CURSOR_HAND);
    lc(PATH_CURSORS "/help.webp",       DMX_CURSOR_HELP);
    lc(PATH_CURSORS "/pencil.webp",     DMX_CURSOR_PENCIL);
    lc(PATH_CURSORS "/size_all.webp",   DMX_CURSOR_SIZEALL);
    lc(PATH_CURSORS "/size_bdiag.webp", DMX_CURSOR_SIZEBDIAG);
    lc(PATH_CURSORS "/size_fdiag.webp", DMX_CURSOR_SIZEFDIAG);
    lc(PATH_CURSORS "/size_hor.webp",   DMX_CURSOR_SIZEHOR);
    lc(PATH_CURSORS "/size_ver.webp",   DMX_CURSOR_SIZEVER);
    lc(PATH_CURSORS "/text.webp",       DMX_CURSOR_TEXT);
    lc(PATH_CURSORS "/up_arrow.webp",   DMX_CURSOR_UPARROW);


    dmx->cursor_index = DMX_CURSOR_ARROW;
    dmx->cursor_x = 0;
    dmx->cursor_y = 0;

    return 0;
}




int main(int argc, char** argv) {

   
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'q'},
        { NULL, 0, NULL, 0 }
    };


    int c, idx;
    while((c = getopt_long(argc, argv, "k", long_options, &idx)) != -1) {
        switch(c) {
            case 'q':
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


    int ld = open("/dev/log", O_WRONLY);
    if(ld >= 0) {
        dup2(ld, STDOUT_FILENO);
        dup2(ld, STDERR_FILENO);
    }


    static dmx_t dmx;
    memset(&dmx, 0, sizeof(dmx_t));


    fprintf(stdout, "dmx: loading...\n");    

    if(init_server(&dmx) != 0)
        return -1;

    if(init_screen(&dmx) != 0)
        return -1;

    if(init_fonts(&dmx) != 0)
        return -1;

    if(init_input(&dmx) != 0)
        return -1;



    pthread_create(&dmx.th_render, NULL, th_render, &dmx);
    pthread_create(&dmx.th_input, NULL, th_input, &dmx);
    
    fprintf(stdout, "dmx: running...\n");
    
#if 1 /* TEST */
    do {
        dmx_gc_t* gc = dmx_gc_alloc(&dmx, getpid(), 300, 300);
        gc->window.x = 200;
        gc->window.y = 200;
        strcpy(gc->window.title, "Hello World");
        
        dmx_wm_draw_borders(&dmx, gc);
        list_push(dmx.clients, gc);
    } while(0);
#endif
    
    for(;;) {
        int type;
        if(read(dmx.fd, &type, sizeof(int)) != sizeof(int)) {
            fprintf(stderr, "dmx: I/O error on " DMX_PIPE ": %s\n", strerror(errno));
            return -1;
        }


        #define PK_ERROR(x, y) {                    \
            fprintf(stderr, "dmx: " y " on " #x);   \
            break;                                  \
        }

        switch(type) {
            case DMX_PROTO_CONNECT: {
                dmx_packet_connection_t pk;
                if(read(dmx.fd, &pk, sizeof(pk) != sizeof(pk)))
                    PK_ERROR(DMX_PROTO_CONNECT, "I/O error");

                dmx_proto_ack(type, pk.pid, NULL);
                break;
            }
            case DMX_PROTO_DISCONNECT: {
                dmx_packet_connection_t pk;
                if(read(dmx.fd, &pk, sizeof(pk) != sizeof(pk)))
                    PK_ERROR(DMX_PROTO_DISCONNECT, "I/O error");
                    
                dmx_proto_disconnect_client(&dmx, &pk);
                break;
            }
            case DMX_PROTO_CREATE_GC: {
                dmx_packet_gc_t pk;
                if(read(dmx.fd, &pk, sizeof(pk) != sizeof(pk)))
                    PK_ERROR(DMX_PROTO_CREATE_GC, "I/O error");
                
                dmx_proto_create_gc(&dmx, &pk);
            }
            case DMX_PROTO_DESTROY_GC: {
                dmx_packet_gc_t pk;
                if(read(dmx.fd, &pk, sizeof(pk) != sizeof(pk)))
                    PK_ERROR(DMX_PROTO_CREATE_GC, "I/O error");
                
                dmx_proto_destroy_gc(&dmx, &pk);
            }
        }
    }

    return 0;
}