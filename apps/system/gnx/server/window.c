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
#include <aplus/gnx.h>

#include "gnxsrv.h"

typedef struct {
    char* value;
    TTF_Font* font;
} label_t;

typedef struct window {
    gnx_hwnd_t hwnd;
    label_t text;
    SDL_Surface* surface;
    int redraw;
    
    gnx_window_t usr;
    struct window* next;
} window_t;

static window_t* wnd_queue = NULL;
static gnx_wid_t wnd_id = 0;



int gnxsrv_window_create(gnx_hwnd_t hwnd, gnx_wid_t parent) {
    window_t* wnd = (window_t*) malloc(sizeof(window_t));
    if(!wnd)
        return -1;
    
    wnd->hwnd = hwnd;
    wnd->text.value = NULL;
    wnd->text.font = NULL;
    wnd->redraw = 1;
    
    wnd->surface = SDL_CreateRGBSurface(
        0, 400, 300, GNX_CurrentDisplay->format->BitsPerPixel,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000
    );
    
    if(!wnd->surface) {
        fprintf(stderr, "gnx-server: cannot create surface for window: %s\n", SDL_GetError());
        return -1;
    }
    
    wnd->usr.w_id = wnd_id++;
    wnd->usr.w_x = GNX_CurrentDisplay->w / 2 - 200;
    wnd->usr.w_y = GNX_CurrentDisplay->h / 2 - 150;
    wnd->usr.w_width = 400;
    wnd->usr.w_height = 300;
    wnd->usr.w_backcolor = SDL_MapRGB(wnd->surface->format, 0xEE, 0xEE, 0xEE);
    wnd->usr.w_forecolor = SDL_MapRGB(wnd->surface->format, 0, 0, 0);
    wnd->usr.w_screen = GNX_CurrentDisplayIndex;
    wnd->usr.w_hided = 0;
    
    wnd->next = wnd_queue;
    wnd_queue = wnd;
    
    if(verbose)
        fprintf(stdout, "gnx-server: created window: %d\n", wnd->usr.w_id);
    
    
    gnxsrv_hwnd_raise(hwnd, wnd->usr.w_id, GNXEV_TYPE_WINDOW_INIT, 0, sizeof(wnd->usr), &wnd->usr);
    return wnd->usr.w_id;
}

int gnxsrv_window_close(gnx_hwnd_t hwnd, gnx_wid_t wid) {
    
    window_t* tmp;
    for(tmp = wnd_queue; tmp; tmp = tmp->next)
        if(tmp->usr.w_id == wid)
            break;
            
    if(!tmp) {
        fprintf(stderr, "gnx-server: invalid window: %lld\n", wid);
        return -1;
    }
    
    if(tmp->text.value)
        free(tmp->text.value);
    if(tmp->text.font)
        gnxsrv_resources_unload(tmp->text.font);
    if(tmp->surface)
        SDL_FreeSurface(tmp->surface);
        
    if(tmp == wnd_queue)
        wnd_queue = tmp->next;
    else {
        window_t* p;
        for(p = wnd_queue; p->next; p = p->next)
            if(p->next == tmp)
                break;
                
        if(!p->next) {
            fprintf(stderr, "gnx-server: BUG!! %s::%s (%d)\n", __FILE__, __func__, __LINE__);
            return -1;
        }
        
        p->next = tmp->next;
    }
    
    return 0;
}

int gnxsrv_window_resize(gnx_hwnd_t hwnd, gnx_wid_t wid) {
    window_t* tmp;
    for(tmp = wnd_queue; tmp; tmp = tmp->next)
        if(tmp->usr.w_id == wid)
            break;
            
    if(!tmp) {
        fprintf(stderr, "gnx-server: invalid window: %lld\n", wid);
        return -1;
    }
    
    if(tmp->surface)
        SDL_FreeSurface(tmp->surface);
        
    tmp->surface = SDL_CreateRGBSurface(
        0, tmp->usr.w_width, tmp->usr.w_height, GNX_Display[tmp->usr.w_screen]->format->BitsPerPixel,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000
    );
    
    
    tmp->redraw = 1;
    return 0;
}

int gnxsrv_window_blit(gnx_hwnd_t hwnd, gnx_wid_t wid) {
    window_t* tmp;
    for(tmp = wnd_queue; tmp; tmp = tmp->next)
        if(tmp->usr.w_id == wid)
            break;
            
    if(!tmp) {
        fprintf(stderr, "gnx-server: invalid window: %lld\n", wid);
        return -1;
    }
    
    
    
    if(tmp->redraw) {
        SDL_Rect r;
        r.x =
        r.y = 0;
        r.w = tmp->usr.w_width;
        r.h = 24;
        
        SDL_FillRect(tmp->surface, NULL, tmp->usr.w_backcolor);
        SDL_FillRect(tmp->surface, &r, tmp->usr.w_backcolor - 0x00101010);
        
        
        if(tmp->text.value && tmp->text.font) {
            r.x += 3;
            r.y += 3;
            
            SDL_Color c;
            SDL_GetRGBA(tmp->usr.w_forecolor, tmp->surface->format, &c.r, &c.g, &c.b, &c.a);
            SDL_BlitSurface(TTF_RenderUTF8_Blended(tmp->text.font, tmp->text.value, c), NULL, tmp->surface, &r);
        }
    }
    
    
    if(tmp->usr.w_screen == GNX_CurrentDisplayIndex) {
        SDL_Rect r;
        r.x = tmp->usr.w_x;
        r.y = tmp->usr.w_y;
        r.w = tmp->usr.w_width;
        r.h = tmp->usr.w_height;
        
        SDL_BlitSurface(tmp->surface, NULL, GNX_CurrentDisplay, &r);
    }
    
    return 0;
}

int gnxsrv_window_set_font(gnx_hwnd_t hwnd, gnx_wid_t wid, char* fontface) {
    window_t* tmp;
    for(tmp = wnd_queue; tmp; tmp = tmp->next)
        if(tmp->usr.w_id == wid)
            break;
            
    if(!tmp) {
        fprintf(stderr, "gnx-server: invalid window: %lld\n", wid);
        return -1;
    }
    
    if(tmp->text.font)
        gnxsrv_resources_unload(tmp->text.font);
        
    if(gnxsrv_resources_load(fontface, GNXRES_TYPE_FONT) != 0)
        return -1;
        
    gnx_res_t* res = gnxsrv_resources_find(fontface);
    tmp->text.font = res->font;
    return 0;
}

int gnxsrv_window_set_title(gnx_hwnd_t hwnd, gnx_wid_t wid, char* title) {
    window_t* tmp;
    for(tmp = wnd_queue; tmp; tmp = tmp->next)
        if(tmp->usr.w_id == wid)
            break;
            
    if(!tmp) {
        fprintf(stderr, "gnx-server: invalid window: %lld\n", wid);
        return -1;
    }
    
    if(tmp->text.value)
        free(tmp->text.value);
        
    tmp->text.value = strdup(title);
    return 0;
}