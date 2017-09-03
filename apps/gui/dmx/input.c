#include "dmx.h"
#include <aplus/base.h>
#include <aplus/input.h>
#include <aplus/event.h>
#include <unistd.h>
#include <fcntl.h>

int init_input(dmx_t* dmx) {
    TRACE("Initializing InputManager\n");

    #define lc(p, i) {                                                              \
        dmx->cursors[i] = cairo_image_surface_create_from_png(p);                   \
        if(!dmx->cursors[i]) {                                                      \
            switch(cairo_surface_status(dmx->cursors[i])) {                         \
                case CAIRO_STATUS_NO_MEMORY:                                        \
                    TRACE("%s: CAIRO_STATUS_NO_MEMORY\n", p);                       \
                    break;                                                          \
                case CAIRO_STATUS_FILE_NOT_FOUND:                                   \
                    TRACE("%s: CAIRO_STATUS_FILE_NOT_FOUND\n", p);                  \
                    break;                                                          \
                case CAIRO_STATUS_READ_ERROR:                                       \
                    TRACE("%s: CAIRO_STATUS_READ_ERROR\n", p);                      \
                    break;                                                          \
            }                                                                       \
                                                                                    \
            return -1;                                                              \
        }                                                                           \
    }

    
    lc(PATH_CURSORS "/arrow.png", DMX_CURSOR_ARROW);
    lc(PATH_CURSORS "/cross.png", DMX_CURSOR_CROSS);
    lc(PATH_CURSORS "/forbidden.png", DMX_CURSOR_FORBIDDEN);
    lc(PATH_CURSORS "/hand.png", DMX_CURSOR_HAND);
    lc(PATH_CURSORS "/help.png", DMX_CURSOR_HELP);
    lc(PATH_CURSORS "/pencil.png", DMX_CURSOR_PENCIL);
    lc(PATH_CURSORS "/size_all.png", DMX_CURSOR_SIZEALL);
    lc(PATH_CURSORS "/size_bdiag.png", DMX_CURSOR_SIZEBDIAG);
    lc(PATH_CURSORS "/size_fdiag.png", DMX_CURSOR_SIZEFDIAG);
    lc(PATH_CURSORS "/size_hor.png", DMX_CURSOR_SIZEHOR);
    lc(PATH_CURSORS "/size_ver.png", DMX_CURSOR_SIZEVER);
    lc(PATH_CURSORS "/text.png", DMX_CURSOR_TEXT);
    lc(PATH_CURSORS "/up_arrow.png", DMX_CURSOR_UPARROW);


    dmx->cursor_index = DMX_CURSOR_ARROW;
    dmx->cursor_x = 0;
    dmx->cursor_y = 0;

    TRACE("Done!\n");
    return 0;
}


void* th_input(void* arg) {
    TRACE("Running\n");
    dmx_t* dmx = (dmx_t*) arg;

    int fd = open("/dev/ev0", O_RDONLY);
    if(fd < 0) {
        TRACE("/dev/ev0: could not open\n");
        pthread_exit(NULL);
    }


    for(;;) {
        event_t e;
        if(read(fd, &e, sizeof(e)) <= 0) {
            TRACE("/dev/ev0: I/O error\n");
            break;
        }

        switch(e.ev_type) {
            case EV_REL: {
                int dx = (int) dmx->cursor_x + e.ev_rel.x;
                int dy = (int) dmx->cursor_y + e.ev_rel.y;

                dmx->cursor_x = dx < 0 ? 0 : (dx > dmx->width ? dmx->width : dx);
                dmx->cursor_y = dy < 0 ? 0 : (dy > dmx->height ? dmx->height : dy);
                dmx->redraw = 1;
                break;
            }
            case EV_KEY:
                /* TODO */
            default:
                continue;
        }

        
    }

    close(fd);
}