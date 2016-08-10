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


int gnxsrv_cursor_update_thread(void* arg) {
    (void) arg;
    
    if(verbose)
        fprintf(stdout, "gnx-server: gnxsrv_cursor_update_thread() running\n");
    
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
        
        fprintf(stdout, "gnx-server: mouse x:%d y:%d\n", m.x, m.y);
    } while(gnxsrv_alive);
    
    close(fd);
}