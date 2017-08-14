#include "../dmx.h"
#include <aplus/input.h>
#include <unistd.h>
#include <fcntl.h>

int init_cursor(dmx_t* dmx) {
    TRACE("Initializing Cursors\n");

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


void* th_cursor(void* arg) {
    TRACE("Running\n");
    dmx_t* dmx = (dmx_t*) arg;

    int fd = open(PATH_MOUSEDEV, O_RDONLY);
    if(fd < 0) {
        TRACE(PATH_MOUSEDEV ": could not open\n");
        pthread_exit(NULL);
    }


    for(;;) {
        mouse_t m;
        if(read(fd, &m, sizeof(m)) <= 0) {
            TRACE(PATH_MOUSEDEV ": I/O error\n");
            break;
        }

        dmx->cursor_x = m.x > dmx->width ? dmx->width : m.x;
        dmx->cursor_y = m.y > dmx->height ? dmx->height : m.y;
        dmx->redraw = 1;
    }

    close(fd);
}