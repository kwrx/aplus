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

    printf("dmx#%d -> screen: %dx%dx%d\n", dmx->cid, dmx->screen.width, dmx->screen.height, dmx->screen.bpp);
    dmx_close(fd);
    
    return 0;
}