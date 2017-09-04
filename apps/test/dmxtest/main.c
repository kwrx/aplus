#include <stdio.h>
#include <aplus/base.h>
#include <aplus/dmx.h>

int main(int argc, char** argv) {
    FILE* fp = fopen("/dev/log", "w");
    stderr = fp;
    stdout = fp;

    //int fd = dmx_open(getenv("DMX"));
    int fd = dmx_open("/tmp/dmxctl");
    if(fd < 0)
        dmx_throw();


    dmx_context_t* dmx;
    if(dmx_context_open(fd, &dmx) != 0)
        dmx_throw();


    dmx_view_set_position(dmx, 300.0, 300.0);
    dmx_view_set_size(dmx, 600.0, 400.0);
    dmx_view_set_alpha(dmx, 1.0);
    dmx_view_set_font(dmx, 0);
    dmx_view_show(dmx);

    memset(dmx->window.frame, 0xFF, 600 * 400 * 4);

    dmx_view_invalidate(dmx);

    sleep(100);
    //dmx_close(fd);
    
    return 0;
}