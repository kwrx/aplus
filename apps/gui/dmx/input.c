#include "dmx.h"
#include <aplus/base.h>
#include <aplus/input.h>
#include <aplus/events.h>
#include <unistd.h>
#include <fcntl.h>


void* th_input(void* arg) {
    dmx_t* dmx = (dmx_t*) arg;

    int fd = open("/dev/ev1", O_RDONLY);
    if(fd < 0) {
        fprintf(stderr, "dmx: /dev/ev1: could not open\n");
        exit(1);
    }


    for(;;) {
        event_t e;
        if(read(fd, &e, sizeof(e)) <= 0) {
            fprintf(stderr, "dmx: /dev/ev1: I/O error\n");
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
    exit(0);
}