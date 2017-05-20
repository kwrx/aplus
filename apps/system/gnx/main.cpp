#include <gnxserver.h>


int main(int argc, char** argv) {
    int fd = open("/dev/fb0", O_RDONLY);
    if(fd < 0) {
        cerr << "/dev/fb0: could not open framebuffer" << endl;
        return -1;
    }
    

    fbdev_mode_t m;
    m.width = (uint16_t) sysconfig("screen.width", SYSCONFIG_FORMAT_INT, 800);
    m.height = (uint16_t) sysconfig("screen.height", SYSCONFIG_FORMAT_INT, 600);
    m.bpp = (uint16_t) sysconfig("screen.bpp", SYSCONFIG_FORMAT_INT, 32);
    m.vx =
    m.vy = 0;
    m.lfbptr = NULL;
    
    ioctl(fd, FBIOCTL_SETMODE, &m);
    ioctl(fd, FBIOCTL_GETMODE, &m);
    close(fd);

    

    //Server* gnx = new Server(m.width, m.height, m.bpp, m.lfbptr);
    
    //gnx->Run();
    return 0;
}