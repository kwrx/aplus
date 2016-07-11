#include "gnx-private.h"

#define check_errors(a, b)              \
    if(b) {                             \
        perror(a);                      \
        return -1;                      \
    }
    
    #define check_sdl_errors(a, b)      \
    if(b) {                             \
        fprintf(stderr, "%s: %s\n",     \
        #a, a##_GetError());            \
        return -1;                      \
    }
    

SDL_Surface* screen = NULL;
SDL_Renderer* screen_render = NULL;
SDL_Surface* wallpaper = NULL;





int main(int argc, char** argv, char** env) {
    
    /* Load Framebuffer */
    fbdev_mode_t mode;
    int fd = open("/dev/fb0", O_RDONLY);
    check_errors("/dev/fb0", fd < 0);
    check_errors("/dev/fb0", ioctl(fd, FBIOCTL_GETMODE, &mode) != 0);
    close(fd);
    
    
    /* Load SDL */
    check_sdl_errors(SDL, SDL_Init(SDL_INIT_EVENTS) != 0);
    check_sdl_errors(TTF, TTF_Init() != 0);
    check_sdl_errors(IMG, IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) != (IMG_INIT_PNG | IMG_INIT_JPG));
    
    
    /* Load screen */
    screen = SDL_CreateRGBSurfaceFrom (
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
    
    check_sdl_errors(SDL, !screen);
    
    /* Load Render*/
    screen_render = SDL_CreateSoftwareRenderer(screen);
    check_sdl_errors(SDL, !screen_render);
    
    /* Load Device */   
    check_errors(GNX_DEVICE, mkfifo(GNX_DEVICE, 0777) != 0);
 
    /* Load Wallpaper */
    wallpaper = __optimize_sdl_surface(IMG_Load(PATH_WALLPAPER), 1);
    check_sdl_errors(SDL, !wallpaper);
    
    /* Draw wallpaper */
    SDL_LockSurface(screen);
    memcpy(screen->pixels, wallpaper->pixels, screen->pitch * screen->h);
    SDL_UnlockSurface(screen);
    
 
 
    
    /* Start GNX Server */
    fd = open(GNX_DEVICE, O_RDWR);
    check_errors(GNX_DEVICE, fd < 0);
    
#if DEBUG
    /* Load test window */
    if(fork() == 0) {
        #define W(x)    write(fd, &x, sizeof(x))
        
        gnx_cmd_t cmd = GNX_WND_CREATE;
        W(cmd);
        
        gnx_handle_t handle = GNX_HANDLE_NULL;
        W(handle);
        
        gnx_rect_t r;
        r.x = 100;
        r.y = 100;
        r.w = 500;
        r.h = 400;
        W(r);
        
        exit(0);
    }
 #endif
    
    while(1) {
        static gnx_cmd_t cmd;
        read(fd, &cmd, sizeof(cmd));

#if DEBUG        
        fprintf(stderr, "gnx: received command <%02d>\n", cmd);
#endif
     
        switch(cmd) {
            case GNX_WND_CREATE: {
                gnx_handle_t handle;
                read(fd, &handle, sizeof(gnx_handle_t));
                
                gnx_rect_t rect;
                read(fd, &rect, sizeof(gnx_rect_t));
                
                //gnx_handle_t* ret;
                //read(fd, &ret, sizeof(gnx_handle_t*));
                
                __gnx_new_window(handle, &rect);
                break;
            }    
            case GNX_WND_GETFLAGS: {
                gnx_window_t* w;
                read(fd, &w, sizeof(gnx_handle_t));
                
                uint32_t* flags;
                read(fd, &flags, sizeof(uint32_t*));
                
                *flags = w->flags;
                break;
            }
            case GNX_WND_DRAW: {
                gnx_handle_t w;
                read(fd, &w, sizeof(gnx_handle_t));
                
                __gnx_draw_window(w);
                break;
            }
            case GNX_WND_DESTROY:
            case GNX_WND_SETRECT:
            case GNX_WND_GETRECT:
            case GNX_WND_SETFLAGS:
            default:
                fprintf(stderr, "gnx: command <%02d> not implemented\n", cmd);
        }
        
        return 0;
    }
   
   
    return 0;
}