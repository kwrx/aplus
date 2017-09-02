#include "../dmx.h"
#include <aplus/input.h>
#include <unistd.h>
#include <fcntl.h>



void* th_keyboard(void* arg) {
    TRACE("Running\n");
    dmx_t* dmx = (dmx_t*) arg;

    int fd = open(PATH_KBDEV, O_RDONLY);
    if(fd < 0) {
        TRACE(PATH_KBDEV ": could not open\n");
        pthread_exit(NULL);
    }

    for(;;) {
        vkey_t vk;
        if(read(fd, &vk, sizeof(vkey_t)) <= 0) {
            TRACE(PATH_MOUSEDEV ": I/O error\n");
            break;
        }

#if 0
        if(likely(dmx->window_focused))
            dmx_raise(dmx->window_focused, DMX_EVENT_KEY, &vk, sizeof(vkey_t));
#endif        
    }
}
