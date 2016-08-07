#include <stdio.h>
#include <aplus/gnx.h>


static gnx_hwnd_t HWND;

static void on_event(int sender, gnx_event_t* e) {
    switch(e->e_type) {
        case GNXEV_TYPE_INIT: {
            HWND = e->e_param;
            
            fprintf(stdout, "GNXEV_TYPE_INIT: %d\n", HWND);
            gnx_exit(0);
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