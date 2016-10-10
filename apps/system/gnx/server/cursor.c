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
#include <aplus/input.h>

#include "gnxsrv.h"


SDL_Surface* GNX_Cursor[] = { NULL };
SDL_Surface* GNX_CurrentCursor = NULL;
int GNX_CurrentCursorIndex = 0;

static int x = 0;
static int y = 0;

int gnxsrv_cursor_update_thread(void* arg) {
    (void) arg;
    
    if(verbose)
        fprintf(stdout, "gnx-server: gnxsrv_cursor_update_thread() running\n");
        
        
    GNX_Cursor[GNXCUR_TYPE_ARROW] = gnxsrv_resources_load("/usr/share/cursor/arrow.cur", GNXRES_TYPE_IMAGE);
    GNX_Cursor[GNXCUR_TYPE_CROSS] = gnxsrv_resources_load("/usr/share/cursor/cross.cur", GNXRES_TYPE_IMAGE);
    GNX_Cursor[GNXCUR_TYPE_FORBIDDEN] = gnxsrv_resources_load("/usr/share/cursor/forbidden.cur", GNXRES_TYPE_IMAGE);
    GNX_Cursor[GNXCUR_TYPE_HELP] = gnxsrv_resources_load("/usr/share/cursor/help.cur", GNXRES_TYPE_IMAGE);
    GNX_Cursor[GNXCUR_TYPE_PENCIL] = gnxsrv_resources_load("/usr/share/cursor/pencil.cur", GNXRES_TYPE_IMAGE);
    GNX_Cursor[GNXCUR_TYPE_HAND] = gnxsrv_resources_load("/usr/share/cursor/pointing_hand.cur", GNXRES_TYPE_IMAGE);
    GNX_Cursor[GNXCUR_TYPE_SIZEALL] = gnxsrv_resources_load("/usr/share/cursor/size_all.cur", GNXRES_TYPE_IMAGE);
    GNX_Cursor[GNXCUR_TYPE_SIZEBDIAG] = gnxsrv_resources_load("/usr/share/cursor/size_bdiag.cur", GNXRES_TYPE_IMAGE);
    GNX_Cursor[GNXCUR_TYPE_SIZEFDIAG] = gnxsrv_resources_load("/usr/share/cursor/size_fdiag.cur", GNXRES_TYPE_IMAGE);
    GNX_Cursor[GNXCUR_TYPE_SIZEHOR] = gnxsrv_resources_load("/usr/share/cursor/size_hor.cur", GNXRES_TYPE_IMAGE);
    GNX_Cursor[GNXCUR_TYPE_SIZEVER] = gnxsrv_resources_load("/usr/share/cursor/size_ver.cur", GNXRES_TYPE_IMAGE);
    GNX_Cursor[GNXCUR_TYPE_TEXT] = gnxsrv_resources_load("/usr/share/cursor/text.cur", GNXRES_TYPE_IMAGE);
    GNX_Cursor[GNXCUR_TYPE_UPARROW] = gnxsrv_resources_load("/usr/share/cursor/up_arrow.cur", GNXRES_TYPE_IMAGE);
   
    
    gnxsrv_cursor_select(GNXCUR_TYPE_ARROW);
    
    
    
    int fd = open(MOUSE_DEVICE, O_RDONLY);
    if(fd < 0) {
        fprintf(stderr, "gnx-server: %s: %s\n", MOUSE_DEVICE, strerror(errno));
        return -1;
    }
    

    do {
        static mouse_t m;
        if(read(fd, &m, sizeof(m)) != sizeof(m)) {
            fprintf(stderr, "gnx-server: %s: %s\n", MOUSE_DEVICE, strerror(errno));
            return -1;
        }
        
        
        x += m.dx;
        y -= m.dy;
        
        x = x < 0 ? 0 : (x < GNX_CurrentDisplay->w ? x : GNX_CurrentDisplay->w);
        y = y < 0 ? 0 : (y < GNX_CurrentDisplay->h ? y : GNX_CurrentDisplay->h);
    
        gnxsrv_cursor_update();
    } while(gnxsrv_alive);
    
    close(fd);
}


int gnxsrv_cursor_update() {
    SDL_Rect r;
    r.x = x;
    r.y = y;
    r.w = GNX_CurrentCursor->w;
    r.h = GNX_CurrentCursor->h;
    GNX_BlitSurface(GNX_CurrentCursor, NULL, GNX_CurrentDisplay, &r);
    
    return 0;
}

int gnxsrv_cursor_select(uint8_t index) {
    if(index > sizeof(GNX_Cursor) / sizeof(SDL_Surface*)) {
        fprintf(stderr, "gnx-server: invalid cursor type: %d\n", index);
        return -1;
    }
    
    GNX_CurrentCursor = GNX_Cursor[index];
    GNX_CurrentCursorIndex = index;
    
    gnxsrv_cursor_update();
    return 0;
}