#include "gnx-private.h"

#define check_errors(a, b)                          \
    fprintf(stderr, "checking for %s: \"%s\"...", a, #b);\
    if(b) {                                         \
        perror(a);                                  \
        return -1;                                  \
    } fprintf(stderr, "OK\n")
    
    #define check_sdl_errors(a, b)                  \
    fprintf(stderr, "checking for %s: \"%s\"...", #a, #b);\
    if(b) {                                         \
        fprintf(stderr, "%s: %s\n",                 \
        #a, a##_GetError());                        \
        return -1;                                  \
    } fprintf(stderr, "OK\n")
    

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
 
    /* Load Wallpaper */
    wallpaper = __optimize_sdl_surface(IMG_Load(PATH_WALLPAPER), 1);
    check_sdl_errors(SDL, !wallpaper);
    
    /* Draw wallpaper */
    SDL_LockSurface(screen);
    memcpy(screen->pixels, wallpaper->pixels, screen->pitch * screen->h);
    SDL_UnlockSurface(screen);
    
    
    /* Start GNX Server */
    struct sockaddr_in tmp;
   
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    check_errors("socket", sd < 0);
    
    tmp.sin_len = sizeof(tmp);
    tmp.sin_family = AF_INET;
    tmp.sin_addr.s_addr = INADDR_ANY;
    tmp.sin_port = __builtin_bswap16(GNX_PORT);
    
    check_errors("bind", bind(sd, (struct sockaddr*) &tmp, sizeof(tmp)) != 0);
    check_errors("listen", listen(sd, 7) != 0);

#if DEBUG
    fprintf(stderr, "gnx: listening connections\n");
#endif

    
    do {
        fd = accept(sd, 0, 0);
        check_errors("accept", fd < 0);
        
#if DEBUG
        fprintf(stderr, "gnx: accepted connection request from %d\n", fd);
#endif
        
        if(fork() == 0)
            break;
    } while(1);
    
    
    do {
        static gnx_cmd_t cmd;
        while(read(fd, &cmd, sizeof(cmd)) != sizeof(cmd))
            sched_yield();

#if DEBUG        
        fprintf(stderr, "gnx: received command <%02d> from %d\n", cmd, fd);
#endif
     
        switch(cmd) {
            case GNX_WND_CREATE: {
                gnx_handle_t handle;
                read(fd, &handle, sizeof(gnx_handle_t));
                
                gnx_rect_t rect;
                read(fd, &rect, sizeof(gnx_rect_t));
                
                gnx_handle_t* ret;
                read(fd, &ret, sizeof(gnx_handle_t*));
                
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
    } while(1);
   
   
    return 0;
}