#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <aplus/fbdev.h>
#include <aplus/gnx.h>

#include "gnx-private.h"


SDL_Surface* screen = NULL;
SDL_Surface* wallpaper = NULL;


static SDL_Surface* __optimize_sdl_surface(SDL_Surface* surface, int resize) {
    
    SDL_Surface* tmp = surface;
    if(tmp->format->BitsPerPixel != screen->format->BitsPerPixel) {
        tmp = SDL_ConvertSurface(surface, screen->format, 0);
        SDL_FreeSurface(surface);
    }
    
    if(!(
        tmp->w == screen->w     &&
        tmp->h == screen->h     &&
        resize
    )) {
    
        SDL_Surface* tmp_scaled = SDL_CreateRGBSurface (
            0,
            screen->w,
            screen->h,
            screen->format->BitsPerPixel,
            0x00FF0000,
            0x0000FF00,
            0x000000FF,
            0xFF000000
        );
        
        SDL_BlitScaled(tmp, NULL, tmp_scaled, NULL);
        SDL_FreeSurface(tmp);
        
        return tmp_scaled;
    }
    
    return tmp;
}

int main(int argc, char** argv, char** env) {
    fbdev_mode_t mode;
    int fd = open("/dev/fb0", O_RDONLY);
    
    #define check_errors(a, b)      \
        if(b) {                     \
            perror(a);              \
            return -1;              \
        }
        
     #define check_sdl_errors(a, b)         \
        if(b) {                             \
            fprintf(stderr, "%s: %s\n",     \
            #a, a##_GetError());            \
            return -1;                      \
        }
        
    
    check_errors("/dev/fb0", fd < 0);
    check_errors("/dev/fb0", ioctl(fd, FBIOCTL_GETMODE, &mode) != 0);
    close(fd);
    
    
    check_sdl_errors(SDL, SDL_Init(SDL_INIT_EVENTS) != 0);
    check_sdl_errors(TTF, TTF_Init() != 0);
    check_sdl_errors(IMG, IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) != (IMG_INIT_PNG | IMG_INIT_JPG));
    
    
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
    
   
    wallpaper = __optimize_sdl_surface(IMG_Load(PATH_WALLPAPER), 1);
    check_sdl_errors(SDL, !wallpaper);
    
    
    SDL_LockSurface(screen);
    memcpy(screen->pixels, wallpaper->pixels, screen->pitch * screen->h);
    SDL_UnlockSurface(screen);
    
    
    //load_cursor();
   
    return 0;
}