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


void init_display(void);
void* th_display(void*);

void init_clients(void);
void* th_clients(void*);

#endif
