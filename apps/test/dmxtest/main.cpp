#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <aplus/base.h>
#include <aplus/fbdev.h>

#include <cairo/cairo.h>

//#define TEST
#include <aplus/utils/unittest.h>


#include <ptk/Frame.h>
#include <ptk/Font.h>

using namespace std;
using namespace ptk;

int main(int argc, char** argv) {
    __unittest_begin();

    FILE* fp = __unittest_neq(fopen("/dev/log", "w"), NULL, FILE*);
    stderr = fp;
    stdout = fp;


    int fd = __unittest(open("/dev/fb0", O_RDONLY), >=, 0, int);
    
    fbdev_mode_t fb;
    fb.width = 1280;
    fb.height = 720;
    fb.bpp = 32;
    fb.vx = fb.vy = 0;
    fb.lfbptr = NULL;

    __unittest_eq(ioctl(fd, FBIOCTL_SETMODE, &fb), 0, int);
    __unittest_eq(ioctl(fd, FBIOCTL_GETMODE, &fb), 0, int);
    __unittest_eq(close(fd), 0, int);


    __unittest_eq(Font::Initialize(), 0, int);

    Frame* Screen = new Frame(fb.lfbptr, fb.width, fb.height, fb.bpp);
    Font* F = new Font("Ubuntu Regular", 32);
    __unittest_eq(F->IsLoaded(), 1, bool);

    cairo_save(Screen->Fx);
    cairo_rectangle(Screen->Fx, 0, 0, Screen->Width, Screen->Height);
    cairo_set_source_rgb(Screen->Fx, 0, 0, 1);
    cairo_fill(Screen->Fx);
    cairo_restore(Screen->Fx);

    F->DrawTo(Screen, "Hello World", 0, 0);
    
    __unittest_eq(Font::Done(), 0, int);
    __unittest_end();
    return 0;
}