#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <aplus/fbdev.h>

#include "../gnx.h"

SDL_Surface* GNX_Display[GNX_MAX_DISPLAY] = { NULL, NULL, NULL, NULL };

static int gnx_init_display(int display) {
    if(display > GNX_MAX_DISPLAY) {
        fprintf(stderr, "gnx: invalid display %d (accepted: 0-%d)\n", display, GNX_MAX_DISPLAY);
        return -1;
    }
    
    if(GNX_Display[display]) {
        fprintf(stderr, "gnx: display %d already opened\n", display);
        return -1;
    }
    
    
    int fd = open("/dev/fb0", O_RDONLY);
    if(fd < 0) {
        fprintf(stderr, "gnx: /dev/fb0: %s\n", strerror(errno));
        return -1;
    }
    
    fbdev_mode_t mode;
    if(ioctl(fd, FBIOCTL_GETMODE, &mode) != 0) {
        fprintf(stderr, "gnx: /dev/fb0: %s\n", strerror(errno));
        return -1;
    }
    
    close(fd);
    
    
    if(!SDL_WasInit(SDL_INIT_EVENTS)) {
        if(SDL_Init(SDL_INIT_EVENTS) != 0) {
            fprintf(stderr, "gnx: unable to initialize SDL Library: %s\n", SDL_GetError());
            return -1;
        }
        
        if(TTF_Init() != 0) {
            fprintf(stderr, "gnx: unable to initialize SDL TFF Library: %s\n", TTF_GetError());
            return -1;
        }
        
        if(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) != (IMG_INIT_PNG | IMG_INIT_JPG)) {
            fprintf(stderr, "gnx: unable to initialize SDL Image Library: %s\n", IMG_GetError());
            return -1;
        }
    }
    
    
    GNX_Display[display] = SDL_CreateRGBSurfaceFrom (
        mode.lfbptr,
        mode.width,
        mode.height,
        mode.bpp,
        mode.width * (mode.bpp / 8),
        0x00FF0000,
        0x0000FF00,
        0x000000FF,
        0xFF000000
    );
    
    if(!GNX_Display[display]) {
        fprintf(stderr, "gnx: can not initialize display %d: %s\n", display, SDL_GetError());
        return -1;
    }
    
    if(verbose)
        fprintf(stderr, "gnx: initialized display %d (%dx%dx%d)\n", display, mode.width, mode.height, mode.bpp);
    
    return 0;
}


static int gnx_fini_display(int display) {
    if(display > GNX_MAX_DISPLAY) {
        fprintf(stderr, "gnx: invalid display %d (accepted: 0-%d)\n", display, GNX_MAX_DISPLAY);
        return -1;
    }
    
    if(!GNX_Display[display]) {
        fprintf(stderr, "gnx: display %d is not opened\n", display);
        return -1;
    }
    
    SDL_FreeSurface(GNX_Display[display]);
    GNX_Display[display] = NULL;
    
    if(verbose)
        fprintf(stdout, "gnx: display %d closed\n", display);
    
    return 0;
}


static int gnx_init_server(int display) {
    if(verbose)
        fprintf(stdout, "gnx: initializing server\n");
        
        
    #define mknod(x, t)                                                 \
    {                                                                   \
        int fd;                                                         \
        if((fd = open(x, O_CREAT | O_RDONLY, t | 0666)) < 0) {          \
            fprintf(stderr, "gnx: %s: %s\n", x, strerror(errno));       \
            return -1;                                                  \
        }                                                               \
                                                                        \
        close(fd);                                                      \
                                                                        \
        if(verbose)                                                     \
            fprintf(stdout, "gnx: created %s\n", x);                    \
    }
    
    
    mknod("/tmp/gnx", S_IFDIR);
    mknod("/tmp/gnx/apps", S_IFDIR);
    mknod("/tmp/gnx/gnx.log", S_IFREG);
    
    if(mkfifo("/tmp/gnx/gnxctl", 0666) != 0) {
        fprintf(stderr, "gnx: /tmp/gnx/gnxctl: %s\n", strerror(errno));
        return -1;
    }
    
    if(verbose)
        fprintf(stdout, "gnx: created %s\n", "/tmp/gnx/gnxctl");
    
    
    if(gnx_init_display(display) != 0)
        return -1;
    
    
    int fd;
    if((fd = gnxctl_open()) < 0) {
        fprintf(stderr, "gnx: /tmp/gnx/gnxctl: %s\n", strerror(errno));
        return -1;
    }
        
        
    if(verbose)
        fprintf(stdout, "gnxctl: waiting for packets\n");
        
    do {
        gnxctl_packet_t* pk;
        if(gnxctl_recv(fd, &pk) != 0) {
            fprintf(stderr, "gnx-server: /tmp/gnx/gnxctl (%d): %s\n", fd, strerror(errno));
            continue;
        }
        
            
        #define CASE(x)                                                 \
            case x:                                                     \
                {                                                       \
                    if(verbose)                                         \
                        fprintf(stdout,                                 \
                            "gnx-server: received packet %s\n", #x);    \
                }    
        
        switch(pk->g_type) {
            CASE(GNXCTL_TYPE_INIT_DISPLAY) {
                gnx_init_display(pk->g_param);
            } break;
            
            CASE(GNXCTL_TYPE_FINI_DISPLAY) {
                gnx_fini_display(pk->g_param);
            }
            
            default:
                fprintf(stderr, "gnx-server: invalid packet type %d\n", pk->g_type);
                break;
        }   
            
            
        free(pk);
    } while(1);
        
    gnxctl_close(fd);
    
    
    /* TODO: Kill all apps */
    
    unlink("/tmp/gnx/gnxctl");
    unlink("/tmp/gnx/gnx.log");
    unlink("/tmp/gnx/apps");
    unlink("/tmp/gnx");  
    return 0;
}


int gnx_init(int display) {
    int fd;
    if((fd = gnxctl_open()) < 0)
        return gnx_init_server(display);
    
    gnxctl_send(fd, GNXCTL_TYPE_INIT_DISPLAY, 0, display, 0, NULL);
    gnxctl_close(fd);
    
    return 0;
}