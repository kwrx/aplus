#include "config.h"
#include <aplus/input.h>

cairo_surface_t* cx_cursors[GNX_CURSOR_COUNT];
int cx_index = 0;
int cx_x = 0;
int cx_y = 0;

static void lc(const char* path, int index) {
    cx_cursors[index] = cairo_image_surface_create_from_png(path);
    if(!cx_cursors[index]) {
        switch(cairo_surface_status(cx_cursors[index])) {
            case CAIRO_STATUS_NO_MEMORY:
                fprintf(stderr, "%s: no memory left\n", path);
                break;
            case CAIRO_STATUS_FILE_NOT_FOUND:
                fprintf(stderr, "%s: file not found\n", path);
                break;
            case CAIRO_STATUS_READ_ERROR:
                fprintf(stderr, "%s: I/O error\n", path);
                break;
        }

        return;
    }

    fprintf(stdout, "gnx: loaded cursor #%d %s\n", index, path);
}

void init_cursor(void) {
    memset(cx_cursors, 0, sizeof(cx_cursors));

    lc(PATH_CURSORS "/arrow.png", GNX_CURSOR_ARROW);
    lc(PATH_CURSORS "/cross.png", GNX_CURSOR_CROSS);
    lc(PATH_CURSORS "/forbidden.png", GNX_CURSOR_FORBIDDEN);
    lc(PATH_CURSORS "/hand.png", GNX_CURSOR_HAND);
    lc(PATH_CURSORS "/help.png", GNX_CURSOR_HELP);
    lc(PATH_CURSORS "/pencil.png", GNX_CURSOR_PENCIL);
    lc(PATH_CURSORS "/size_all.png", GNX_CURSOR_SIZEALL);
    lc(PATH_CURSORS "/size_bdiag.png", GNX_CURSOR_SIZEBDIAG);
    lc(PATH_CURSORS "/size_fdiag.png", GNX_CURSOR_SIZEFDIAG);
    lc(PATH_CURSORS "/size_hor.png", GNX_CURSOR_SIZEHOR);
    lc(PATH_CURSORS "/size_ver.png", GNX_CURSOR_SIZEVER);
    lc(PATH_CURSORS "/text.png", GNX_CURSOR_TEXT);
    lc(PATH_CURSORS "/up_arrow.png", GNX_CURSOR_UPARROW);

    int fd = open(PATH_MOUSEDEV, O_RDONLY);
    if(fd < 0) {
        perror(PATH_MOUSEDEV);
        return;
    }


}


void* th_cursor(void* arg) {
    fprintf(stdout, "gnx: initialized cursor controller: #%d\n", getpid());

    int fd = open(PATH_MOUSEDEV, O_RDONLY);
    if(fd < 0) {
        perror(PATH_MOUSEDEV);
        pthread_exit(NULL);
    }

    for(;;) {
        mouse_t m;
        if(read(fd, &m, sizeof(m)) <= 0) {
            perror(PATH_MOUSEDEV);
            continue;
        }

        cx_x = m.x;
        cx_y = m.y;
        global_dirty = 1;
    }

    close(fd);
}