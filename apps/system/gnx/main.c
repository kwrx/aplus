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
#include <aplus/fbdev.h>
#include <aplus/msg.h>
#include <aplus/input.h>
#include <aplus/gnx.h>


typedef struct gnxsrv_context {
    int dirty;
    uint32_t flags;
    uint16_t x;
    uint16_t y;
    uint16_t height;
    uint32_t stride;
    void* pixels;

    struct gnxsrv_context* next;
} gnxsrv_context_t;




static void show_usage(int argc, char** argv) {
    printf(
        "Use: gnx [OPTIONS...]\n"
        "Graphical UI Server\n\n"
        "   -k, --kill                  kill all running server\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus coreutils) 0.1\n"
        "Copyright (c) 2016 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}



static int fb_device = -1;
static fbdev_mode_t fb_vidmod;
static gnxsrv_context_t* g_contexts = NULL;
static void* g_buffer = NULL;
static int g_size = 0;
static int g_dirty = 0;


static void t_msg(int sig) {
    static char bufmsg[BUFSIZ];

    pid_t pid;
    while(msg_recv(&pid, bufmsg, BUFSIZ) > 0) {
        register gnxprot_t* p = (gnxprot_t*) bufmsg;

        switch(p->type) {
            case GNXPROT_TYPE_TEST:
                msg_send(pid, p, 2);
                
                break;
            case GNXPROT_TYPE_SETVID:
                fb_vidmod.width = p->data.vidmod.width;
                fb_vidmod.height = p->data.vidmod.height;
                fb_vidmod.bpp = p->data.vidmod.bpp;
                fb_vidmod.vx =
                fb_vidmod.vy = 0;
                ioctl(fb_device, FBIOCTL_SETMODE, &fb_vidmod);

                break;
            case GNXPROT_TYPE_GETVID:
                p->data.vidmod.width = fb_vidmod.width;
                p->data.vidmod.height = fb_vidmod.height;
                p->data.vidmod.bpp = fb_vidmod.bpp;
                msg_send(pid, p, sizeof(p->data.vidmod));

                break;
            case GNXPROT_TYPE_UPDATE_CONTEXT: {
                gnxsrv_context_t* c;
                if(!p->data.context.id) {
                    c = (gnxsrv_context_t*) calloc(sizeof(gnxsrv_context_t), 1);

                    c->next = g_contexts;
                    g_contexts = c;

                    p->data.context.id = (void*) c;
                } else
                    c = (gnxsrv_context_t*) p->data.context.id;

                c->stride = p->data.context.width * (fb_vidmod.bpp >> 3);
                c->height = p->data.context.height;
                c->x = p->data.context.x;
                c->y = p->data.context.y;
                c->pixels = p->data.context.pixels;
                c->flags = p->data.context.flags;

                msg_send(pid, p, sizeof(p->data.context));
            } break;
            case GNXPROT_TYPE_BLIT_CONTEXT: {
                uintptr_t lfbptr = (uintptr_t) g_buffer + (p->data.context.x * (fb_vidmod.bpp >> 3)) + (p->data.context.y * (fb_vidmod.width * (fb_vidmod.bpp >> 3)));
                uintptr_t pixels = (uintptr_t) p->data.context.pixels;

                uintptr_t fbstride = fb_vidmod.width * (fb_vidmod.bpp >> 3);
                uintptr_t wnstride = p->data.context.width * (fb_vidmod.bpp >> 3);
                for(int y; y < p->data.context.height; y++, lfbptr += fbstride, pixels += wnstride)
                    memcpy((void*) lfbptr, (void*) pixels, wnstride);

                gnxsrv_context_t* c = (gnxsrv_context_t*) p->data.context.id;
                c->flags |= GNX_FLAGS_DIRTY;
                g_dirty++;
            } break;
                
        }
    }
}



static void* t_ms(void* arg) {
    int fd = open("/dev/mouse", O_RDONLY);
    if(fd < 0) {
        perror("/dev/mouse");
        pthread_exit(NULL);
    }


    gnxsrv_context_t* c = (gnxsrv_context_t*) calloc(sizeof(gnxsrv_context_t), 1);

    c->stride = 32 * 4;
    c->height = 32;
    c->x = 0;
    c->y = 0;
    c->pixels = malloc(32 * 32 * 4);
    c->flags = GNX_FLAGS_SHOW | GNX_FLAGS_DIRTY;

    c->next = g_contexts;
    g_contexts = c;

    memset(c->pixels, 0xAA, 32 * 32 * 4);

    for(;;);
}



int main(int argc, char** argv) {
   
    static struct option long_options[] = {
        { "kill", no_argument, NULL, 'k'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'q'},
        { NULL, 0, NULL, 0 }
    };


    int c, idx;
    while((c = getopt_long(argc, argv, "k", long_options, &idx)) != -1) {
        switch(c) {
            case 'k':
                unlink("/tmp/gnx.lock");
                return 0;
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


    int fd;
    if((fd = open("/tmp/gnx.lock", O_CREAT | O_EXCL | O_RDWR, S_IFREG | 0644)) < 0) {
        if(errno == EEXIST)
            fprintf(stderr, "Error: server already running, try to close with:\n\tgnx --kill\nor delete lock file:\n\trm -f /tmp/gnx.lock\n");
        else
            perror("/tmp/gnx.lock");

        exit(1);
    }


    pid_t pid = getpid();
    write(fd, &pid, sizeof(pid));
    close(fd);



    char* s;
    if(!(s = getenv("DISPLAY")))
        s = "/dev/fb0";

    
    fb_device = open(s, O_RDONLY);
    if(fb_device < 0) {
        perror(s);
        exit(1);
    }



    fb_vidmod.width = sysconfig("screen.width", SYSCONFIG_FORMAT_INT, 800);
    fb_vidmod.height = sysconfig("screen.height", SYSCONFIG_FORMAT_INT, 600);
    fb_vidmod.bpp = sysconfig("screen.bpp", SYSCONFIG_FORMAT_INT, 32);
    fb_vidmod.vx =
    fb_vidmod.vy = 0;

    ioctl(fb_device, FBIOCTL_SETMODE, &fb_vidmod);
    ioctl(fb_device, FBIOCTL_GETMODE, &fb_vidmod);


    FILE* stdlog = fopen("/dev/log", "w");
    if(stdlog) {
        stderr =
        stdin =
        stdout = stdlog;
    }



    g_size = fb_vidmod.width * fb_vidmod.height * (fb_vidmod.bpp >> 3);
    g_buffer = (void*) malloc(g_size);
    g_dirty = 0;


    pthread_t t_kbdid, t_msid;
    //pthread_create(&t_kbdid, NULL, t_kbd, NULL);
    pthread_create(&t_msid, NULL, t_ms, NULL);
    signal(SIGMSG, t_msg);

    
    for(;;) {
        struct stat st;
        if(stat("/tmp/gnx.lock", &st) != 0 && errno == ENOENT)
            break;

        gnxsrv_context_t* tmp;
        for(tmp = g_contexts; tmp; tmp = tmp->next) {
            if((tmp->flags & GNX_FLAGS_DIRTY) && tmp->flags & GNX_FLAGS_SHOW) {
                tmp->flags &= ~GNX_FLAGS_DIRTY;
                g_dirty++;
            }
        }


        if(g_dirty) {
            g_dirty = 0;

            memcpy(fb_vidmod.lfbptr, g_buffer, g_size);
            memset(g_buffer, 0, g_size);
        }

        usleep(16666);
    }


    close(fb_device);
    fclose(stdlog);
    return 0;
}