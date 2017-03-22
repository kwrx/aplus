#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#define GSHM_INCLUDE_OPERATOR
#include <aplus/gshm.h>
#include <aplus/base.h>
#include <aplus/input.h>
#include <aplus/fbdev.h>
#include <aplus/sysconfig.h>


#include <gnx/Screen.h>
#include <gnx/Window.h>
#include <gnx/Server.h>
#include <gnx/InputController.h>

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

    

    Server* gnx = new Server(m.width, m.height, m.bpp, m.lfbptr);
    gnx->Initialize();

    /* DEBUG */
    Window* Win1 = new Window(gnx->Desktop, "Win1", 600, 400);
    Window* Win2 = new Window(gnx->Desktop, "Win2", 400, 300);

    gnx->Desktop->Paint();


   
    gnx->Run();
    return 0;
}