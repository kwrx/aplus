#ifndef _GNX_PRIVATE_H
#define _GNX_PRIVATE_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <aplus/fbdev.h>
#include <aplus/gnx.h>

#define DEBUG   1

extern SDL_Surface* screen;
extern SDL_Surface* wallpaper;
extern SDL_Renderer* screen_render;

#define PATH_FONTS          "/usr/share/fonts"
#define PATH_ICONS          "/usr/share/icons"
#define PATH_IMAGES         "/usr/share/images"
#define PATH_WALLPAPER      "/usr/share/images/wp.png"



typedef struct gnx_window {
    uint64_t id;
    uint32_t flags;
    SDL_Surface* surface;
    SDL_Renderer* render;
    SDL_Rect position;
    
    struct gnx_window* next;
    struct gnx_window* parent;
} gnx_window_t;


extern gnx_handle_t __gnx_new_window(gnx_handle_t handle, gnx_rect_t* rect);
extern void __gnx_draw_window(gnx_handle_t handle);


static inline SDL_Surface* __optimize_sdl_surface(SDL_Surface* surface, int resize) {
    
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

#endif