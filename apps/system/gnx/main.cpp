#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <unistd.h>
#include "context.h"
#include <aplus/gnx.h>
#include <aplus/fbdev.h>

extern "C" int ioctl(int, int, void*);

int main(int argc, char** argv) {
    
    fbdev_mode_t mode;
    int fd = open("/dev/fb0", O_RDONLY);    
    ioctl(fd, FBIOCTL_GETMODE, &mode);
    close(fd);
    
    
    
    GnxWindow* W = new GnxWindow(NULL, mode.width, mode.height, NULL);
    CTX_NEW_FROM_DATA(W, (unsigned char*) mode.lfbptr, mode.width, mode.height);
    
    GnxWindow* C = new GnxWindow(W, 400, 300);
    C->X = 200;
    C->Y = 200;
    C->Background = &GnxColors::WindowForeground;
    
    W->Childrens->push_back(C);
    
    C->Paint();
    W->Paint();
    
    return 0;
}