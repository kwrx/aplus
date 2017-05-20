#include <aplus/base.h>
#include <aplus/fbdev.h>
#include <aplus/gnx.h>

Gnx::Gnx() {
    this->Displays = new vector<GnxDisplay*>();
    this->CurrentDisplay = NULL;
}

Gnx::~Gnx() {
    delete this->Displays;
}

GnxDisplay* Gnx::OpenDisplay(const char* name) {
    int fd = open(name, O_RDONLY);
    if(fd < 0)
        return -1;

    fbdev_mode_t m;
    m.width = sysconfig("screen.width", SYSCONFIG_FORMAT_INT, 800);
    m.height = sysconfig("screen.height", SYSCONFIG_FORMAT_INT, 600);
    m.bpp = sysconfig("screen.bpp", SYSCONFIG_FORMAT_INT, 32);
    m.vx =
    m.vy = 0;

    if(ioctl(fd, FBIOCTL_SETMODE, &m) != 0)
        return -1;

    close(fd);
    return this->OpenDisplay(m.width, m.height, m.bpp, m.lfbptr);
}

GnxDisplay* Gnx::OpenDisplay(uint16_t w, uint16_t h, uint16_t b, void* data) {
    GnxDisplay* d = new GnxDisplay() {
        Width : w,
        height : h,
        Bpp : b,
        Pixels : data,
        BackBuffer : new char[w * h * (b / 8)]
    };

    if(!this->CurrentDisplay)
        this->CurrentDisplay = d;

    this->Displays->push_back(d);
    return d;
}