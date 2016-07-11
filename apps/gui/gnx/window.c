#include "gnx-private.h"

static gnx_window_t* gnx_windows = NULL;
static gnx_window_t* gnx_topmost = NULL;


gnx_handle_t __gnx_new_window(gnx_handle_t parent, gnx_rect_t* rect) {
    gnx_window_t* w = (gnx_window_t*) calloc(sizeof(gnx_window_t), 1);
    if(!w) {
        errno = ENOMEM;
        return GNX_HANDLE_NULL;
    }
    
    w->surface = SDL_CreateRGBSurface(
        0,
        rect->w,
        rect->h,
        screen->format->BitsPerPixel,
        screen->format->Rmask,
        screen->format->Gmask,
        screen->format->Bmask,
        screen->format->Amask
    );
    
    if(!w->surface) {
        errno = EINVAL;
        return GNX_HANDLE_NULL;
    }
    
    w->render = SDL_CreateSoftwareRenderer(w->surface);
    if(!w->render) {
        errno = EINVAL;
        return GNX_HANDLE_NULL;
    }
    
    memcpy(&w->position, rect, sizeof(gnx_rect_t));
    
    SDL_SetRenderDrawColor(w->render, 0, 0, 0, 255);
    SDL_RenderClear(w->render);
    
    
    static uint64_t win_id = 1;
    
    w->id = win_id++;
    w->flags = 0;
    w->next = gnx_windows;
    w->parent = (gnx_window_t*) parent;
    
    gnx_windows = w;
    return (gnx_handle_t) w;
}

void __gnx_draw_window(gnx_handle_t handle) {
    register gnx_window_t* w = (gnx_window_t*) handle;
    
    SDL_BlitSurface(w->surface, NULL, screen, &w->position);
}