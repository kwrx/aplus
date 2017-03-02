#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#include <aplus/gshm.h>
#include <aplus/fbdev.h>
#include <aplus/sysconfig.h>
#include "gnx.h"

using namespace std;
using namespace GNX;



int main(int argc, char** argv) {
    int fd = open("/dev/fb0", O_RDONLY);
    if(fd < 0) {
        cerr << "/dev/fb0: could not open framebuffer" << endl;
        return -1;
    }
    
    
    fbdev_mode_t m;
    m.width = (uint16_t) sysconfig("screen.width", SYSCONFIG_FORMAT_INT, Screen::Width);
    m.height = (uint16_t) sysconfig("screen.height", SYSCONFIG_FORMAT_INT, Screen::Height);
    m.bpp = (uint16_t) sysconfig("screen.bpp", SYSCONFIG_FORMAT_INT, Screen::Bpp);
    m.vx =
    m.vy = 0;
    m.lfbptr = NULL;
    
    ioctl(fd, FBIOCTL_SETMODE, &m);
    ioctl(fd, FBIOCTL_GETMODE, &m);
    close(fd);
    
    
    Screen::Width = m.width;
    Screen::Height = m.height;
    Screen::Bpp = m.bpp;
    Screen::Stride = m.width * (m.bpp >> 3);
    Screen::FrameBuffer = m.lfbptr;
    
    
    Window* Desktop = new Window(NULL, "Desktop", Screen::Width, Screen::Height);
    Window* Win1 = new Window(Desktop, "Win1", 600, 400);
    Window* Win2 = new Window(Desktop, "Win2", 400, 300);
    Window* Win3 = new Window(Desktop, "Win3", 400, 300);

    
    
    //mx(Desktop);
    mx(Win1);
    mx(Win2);
    mx(Win3);
    
  
 
    return 0;
}