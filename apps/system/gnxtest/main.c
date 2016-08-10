#include <stdio.h>
#include <aplus/gnx.h>


static gnx_hwnd_t HWND;

static void on_event(int sender, gnx_event_t* e) {
    switch(e->e_type) {
        case GNXEV_TYPE_INIT: {
            gnx_window(-1);
        } break;
        
        case GNXEV_TYPE_WINDOW_INIT: {
            gnx_window_set_font(e->e_wid, "/usr/share/fonts/default.ttf");
            gnx_window_set_title(e->e_wid, "MainWindow");
            gnx_window_blit(e->e_wid);
        } break;
        
        default:
            fprintf(stderr, "Invalid EVENT TYPE %d", e->e_type);
            gnx_exit(-1);
    }
}

int main(int argc, char** argv) {
    gnx_init("aplus.gnxtest", on_event);
    
    for(;;) sched_yield();
}