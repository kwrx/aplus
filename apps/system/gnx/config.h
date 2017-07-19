#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <sys/sched.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <aplus/base.h>
#include <aplus/sysconfig.h>
#include <aplus/kmem.h>
#include <aplus/fbdev.h>
#include <aplus/msg.h>
#include <aplus/input.h>
#include <aplus/gnx.h>

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>


#define GNX_PIPE                "/tmp/gnxctl"


typedef struct client {
    struct __gnx_context* data;
    cairo_t* cr;
    cairo_surface_t* surface;

    struct client* next;
} client_t;

client_t* client_queue;
int surface_format;
int surface_bpp;
int global_dirty;
int cx_index;
int cx_x;
int cx_y;
cairo_surface_t* cx_cursors[GNX_CURSOR_COUNT];


void init_display(void);
void* th_display(void*);

void init_clients(void);
void* th_clients(void*);

void init_cursor(void);
void* th_cursor(void*);


void display_draw_surface(cairo_surface_t* surface, int x, int y);

#endif
