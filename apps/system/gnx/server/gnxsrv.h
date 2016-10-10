#ifndef _GNXSRV_H
#define _GNXSRV_H

#include <sys/types.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>


typedef struct gnx_res {
    char* name;
    int type;
    union {
        void* data;
        SDL_Surface* image;
        TTF_Font* font;   
    };
    int refcount;
    struct gnx_res* next;
} __attribute__((packed)) gnx_res_t;


#define GNX_BlitSurface(a, b, c, d)     \
    {                                   \
        c->userdata = (void*) 1;        \
        SDL_BlitSurface(a, b, c, d);    \
    }

extern int verbose;
extern int gnxsrv_alive;
extern int GNX_CurrentDisplayIndex;
extern SDL_Surface* GNX_Display[];
extern SDL_Surface* GNX_CurrentDisplay;


int gnxsrv_init(int display);


gnx_res_t* gnxsrv_resources_find(const char* name);
int gnxsrv_resources_unload(void* data);
int gnxsrv_resources_unload_by_name(const char* name);
void* gnxsrv_resources_load(const char* name, int type);

int gnxsrv_create_hwnd(char* appname, pid_t pid);
int gnxsrv_close_hwnd(gnx_hwnd_t hwnd);
int gnxsrv_hwnd_raise(gnx_hwnd_t hid, gnx_wid_t wid, uint8_t type, gnx_param_t param, size_t dlen, void* data);

int gnxsrv_window_create(gnx_hwnd_t hwnd, gnx_wid_t parent);
int gnxsrv_window_close(gnx_wid_t wid);
int gnxsrv_window_resize(gnx_wid_t wid);
int gnxsrv_window_blit(gnx_wid_t wid);
int gnxsrv_window_set_font(gnx_wid_t wid, char* fontface);
int gnxsrv_window_set_title(gnx_wid_t wid, char* title);
int gnxsrv_window_update();

int gnxsrv_cursor_update_thread(void* arg);
int gnxsrv_cursor_select(uint8_t index);
int gnxsrv_cursor_update();

#endif
