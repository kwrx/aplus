#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sched.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <aplus/fbdev.h>
#include <aplus/gnx.h>

#include "gnxsrv.h"

SDL_Surface* GNX_Display[GNX_MAX_DISPLAY] = { NULL, NULL, NULL, NULL };
SDL_Surface* GNX_CurrentDisplay = NULL;
SDL_Surface* GNX_PDisplay = NULL;
int GNX_CurrentDisplayIndex = -1;
int gnxsrv_alive = 1;


int gnxsrv_pdisplay_update_thread(void* arg) {
    (void) arg;
    
     if(verbose)
        fprintf(stdout, "gnx-server: gnxsrv_pdisplay_update_thread() running\n");
       

#define FPS 1
#if FPS   
    int t0 = time(NULL);
    int fps = 0;
    clock_t cs = clock();
#endif

    uint32_t clear_color = SDL_MapRGB(GNX_CurrentDisplay->format, 0, 0, 0);

    for(; gnxsrv_alive; sched_yield()) {
        if(!GNX_CurrentDisplay->userdata)
            continue;
            
        gnxsrv_window_update();
        gnxsrv_cursor_update();
        GNX_CurrentDisplay->userdata = NULL;
        
        
        SDL_BlitSurface(GNX_CurrentDisplay, NULL, GNX_PDisplay, NULL);
        SDL_FillRect(GNX_CurrentDisplay, NULL, clear_color);
#if FPS       
        fps++;
        if(t0 != time(NULL)) {
            fprintf(stderr, "FPS: %d; CPU: %d %%\n", fps, (clock() - cs) / 10);
            fps = 0;
            cs = clock();
            t0 = time(NULL);
        }
#endif      
    }
}

static int gnxsrv_init_display(int display) {
    if(display > GNX_MAX_DISPLAY) {
        fprintf(stderr, "gnx-server: invalid display %d (accepted: 0-%d)\n", display, GNX_MAX_DISPLAY);
        return -1;
    }
    
    if(GNX_Display[display]) {
        fprintf(stderr, "gnx-server: display %d already opened\n", display);
        return -1;
    }
    
    
    int fd = open("/dev/fb0", O_RDONLY);
    if(fd < 0) {
        fprintf(stderr, "gnx-server: /dev/fb0: %s\n", strerror(errno));
        return -1;
    }
    
    fbdev_mode_t mode;
    if(ioctl(fd, FBIOCTL_GETMODE, &mode) != 0) {
        fprintf(stderr, "gnx-server: /dev/fb0: %s\n", strerror(errno));
        return -1;
    }
    
    close(fd);
    
    
    if(!SDL_WasInit(SDL_INIT_EVENTS)) {
        if(SDL_Init(SDL_INIT_EVENTS) != 0) {
            fprintf(stderr, "gnx-server: unable to initialize SDL Library: %s\n", SDL_GetError());
            return -1;
        }
        
        if(TTF_Init() != 0) {
            fprintf(stderr, "gnx-server: unable to initialize SDL TFF Library: %s\n", TTF_GetError());
            return -1;
        }
        
        if(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) != (IMG_INIT_PNG | IMG_INIT_JPG)) {
            fprintf(stderr, "gnx-server: unable to initialize SDL Image Library: %s\n", IMG_GetError());
            return -1;
        }
    }
    
    if(!GNX_PDisplay) {
        GNX_PDisplay = SDL_CreateRGBSurfaceFrom (
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
        
        //SDL_SetSurfaceBlendMode(GNX_PDisplay, SDL_BLENDMODE_NONE);
    }
    
    GNX_Display[display] = SDL_CreateRGBSurface (
        0,
        mode.width,
        mode.height,
        mode.bpp,
        0x00FF0000,
        0x0000FF00,
        0x000000FF,
        0xFF000000
    );
    
    //SDL_SetSurfaceBlendMode(GNX_Display[display], SDL_BLENDMODE_NONE);

    
    if(!GNX_Display[display]) {
        fprintf(stderr, "gnx-server: can not initialize display %d: %s\n", display, SDL_GetError());
        return -1;
    }
    
    if(verbose)
        fprintf(stderr, "gnx-server: initialized display %d (%dx%dx%d)\n", display, mode.width, mode.height, mode.bpp);
    
    return 0;
}


static int gnxsrv_fini_display(int display) {
    if(display > GNX_MAX_DISPLAY) {
        fprintf(stderr, "gnx-server: invalid display %d (accepted: 0-%d)\n", display, GNX_MAX_DISPLAY);
        return -1;
    }
    
    if(!GNX_Display[display]) {
        fprintf(stderr, "gnx-server: display %d is not opened\n", display);
        return -1;
    }
    
    if(GNX_CurrentDisplay = GNX_Display[display]) {
        fprintf(stderr, "gnx-server: display %d is set as default, it can't be closed\n", display);
        return -1;
    }
    
    SDL_FreeSurface(GNX_Display[display]);
    GNX_Display[display] = NULL;
    
    if(verbose)
        fprintf(stdout, "gnx-server: display %d closed\n", display);
    
    return 0;
}

static int gnxsrv_select_display(int display) {
    if(display > GNX_MAX_DISPLAY) {
        fprintf(stderr, "gnx-server: invalid display %d (accepted: 0-%d)\n", display, GNX_MAX_DISPLAY);
        return -1;
    }
    
    if(!GNX_Display[display]) {
        fprintf(stderr, "gnx-server: display %d is not opened\n", display);
        return -1;
    }
    
    GNX_CurrentDisplay = GNX_Display[display];
    GNX_CurrentDisplayIndex = display;
    
    if(verbose)
        fprintf(stdout, "gnx-server: display %d set as default\n", display);
    
    return 0;
}


static int gnxsrv_init_server(int display) {
    if(verbose)
        fprintf(stdout, "gnx-server: initializing server\n");
        
        
    #define mknod(x, t)                                                     \
    {                                                                       \
        int fd;                                                             \
        if((fd = open(x, O_CREAT | O_RDONLY, t | 0666)) < 0) {              \
            fprintf(stderr, "gnx-server: %s: %s\n", x, strerror(errno));    \
            return -1;                                                      \
        }                                                                   \
                                                                            \
        close(fd);                                                          \
                                                                            \
        if(verbose)                                                         \
            fprintf(stdout, "gnx-server: created %s\n", x);                 \
    }
    
    
    mknod("/tmp/gnx", S_IFDIR);
    mknod("/tmp/gnx/apps", S_IFDIR);
    mknod("/tmp/gnx/gnx.log", S_IFREG);
    
    if(mkfifo("/tmp/gnx/gnxctl", 0666) != 0) {
        fprintf(stderr, "gnx-server: /tmp/gnx/gnxctl: %s\n", strerror(errno));
        return -1;
    }
    
    if(verbose)
        fprintf(stdout, "gnx-server: created %s\n", "/tmp/gnx/gnxctl");
    
    
    if(gnxsrv_init_display(display) != 0)
        return -1;
        
    if(gnxsrv_select_display(display) != 0)
        return -1;
    
    
    if(verbose)
        fprintf(stdout, "gnx-server: initializing cursor thread\n");
    
    if(clone(gnxsrv_cursor_update_thread, NULL, CLONE_FILES | CLONE_FS | CLONE_SIGHAND | CLONE_VM, NULL) < 0) {
        fprintf(stderr, "gnx-server: clone(): %s\n", strerror(errno));
        return -1;
    }
    
    if(verbose)
        fprintf(stdout, "gnx-server: initializing doublebuffer thread\n");
    
    if(clone(gnxsrv_pdisplay_update_thread, NULL, CLONE_FILES | CLONE_FS | CLONE_SIGHAND | CLONE_VM, NULL) < 0) {
        fprintf(stderr, "gnx-server: clone(): %s\n", strerror(errno));
        return -1;
    }
    
    
    int fd;
    if((fd = gnxctl_open()) < 0) {
        fprintf(stderr, "gnx-server: /tmp/gnx/gnxctl: %s\n", strerror(errno));
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
            CASE(GNXCTL_TYPE_KILL_SERVER) {
                gnxsrv_alive = 0;
            } break;
            
            CASE(GNXCTL_TYPE_INIT_DISPLAY) {
                gnxsrv_init_display(pk->g_param);
            } break;
            
            CASE(GNXCTL_TYPE_FINI_DISPLAY) {
                gnxsrv_fini_display(pk->g_param);
            } break;
            
            CASE(GNXCTL_TYPE_SELECT_DISPLAY) {
                gnxsrv_select_display(pk->g_param);
            } break;
            
            CASE(GNXCTL_TYPE_LOAD_RESOURCE) {
                gnxsrv_resources_load(pk->g_data, pk->g_param);
            } break;
            
            CASE(GNXCTL_TYPE_UNLOAD_RESOURCE) {
                gnxsrv_resources_unload_by_name(pk->g_data);
            } break;
            
            CASE(GNXCTL_TYPE_CREATE_HWND) {
                gnxsrv_create_hwnd(pk->g_data, pk->g_param);
            } break;
            
            CASE(GNXCTL_TYPE_CLOSE_HWND) {
                gnxsrv_close_hwnd(pk->g_param);
            } break;
            
            CASE(GNXCTL_TYPE_CREATE_WINDOW) {
                gnxsrv_window_create(pk->g_hwnd, pk->g_param);
            } break;
            
            CASE(GNXCTL_TYPE_CLOSE_WINDOW) {
                gnxsrv_window_close(pk->g_param);
            } break;
            
            CASE(GNXCTL_TYPE_BLIT_WINDOW) {
                gnxsrv_window_blit(pk->g_param);
            } break;
            
            CASE(GNXCTL_TYPE_SET_WINDOW_FONT) {
                gnxsrv_window_set_font(pk->g_param, pk->g_data);
            } break;
            
            CASE(GNXCTL_TYPE_SET_WINDOW_TITLE) {
                gnxsrv_window_set_title(pk->g_param, pk->g_data);
            } break;
            
            
            default:
                fprintf(stderr, "gnx-server: invalid packet type %d\n", pk->g_type);
                break;
        }   
            
            
        free(pk);
    } while(gnxsrv_alive);
    
    
        
    gnxctl_close(fd);
    
    if(verbose)
       fprintf(stdout, "gnx-server: removing files\n");
    
    
    unlink("/tmp/gnx/gnxctl");
    unlink("/tmp/gnx/gnx.log");
    unlink("/tmp/gnx/apps");
    unlink("/tmp/gnx");  
   
    
    if(verbose)
       fprintf(stdout, "gnx-server: closed\n");
       
    return 0;
}


int gnxsrv_init(int display) {
    int fd;
    if((fd = gnxctl_open()) < 0)
        return gnxsrv_init_server(display);
    
    gnxctl_send(fd, GNXCTL_TYPE_INIT_DISPLAY, 0, display, 0, NULL);
    gnxctl_close(fd);
    
    return 0;
}