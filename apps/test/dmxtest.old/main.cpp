#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <aplus/base.h>
#include <aplus/fbdev.h>

#include <cairo/cairo.h>
#include <cairo-ext/cairo-webp.h>

//#define TEST
#include <aplus/utils/unittest.h>
#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/types.h>

#include <ptk/Frame.h>
#include <ptk/Font.h>
#include <ptk/Window.h>

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

    ptk::Window* Window = new ptk::Window();
    Window->Width = (double) (fb.width - 1);
    Window->Height = (double) (fb.height - 1);
    Window->Title = "Hello World";

    Window->OnCreate = [&] (auto W) -> void {
        W->View = new Frame(fb.lfbptr, fb.width, fb.height, fb.bpp);
        W->View->Paint([&] (auto cr) -> void {
            cairo_save(cr);
            cairo_rectangle(cr, 0.0, 0.0, (double) fb.width, (double) fb.height);
            cairo_set_source_rgba(cr, 0, 0, 0.0, 1.0);
            cairo_fill(cr);
        });
    };

    Window->OnPaint = [] (auto W, auto cr) -> void {
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
        cairo_paint(cr);

        cairo_save(cr);
        cairo_set_font_face(cr, Font::Load("Ubuntu Regular"));
        cairo_set_font_size(cr, 64.0);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        cairo_move_to(cr, 300.0, 300.0);
        cairo_show_text(cr, "Hello World");
        cairo_restore(cr);
    };

    Window->Show();
    
    __unittest_eq(Font::Done(), 0, int);
    __unittest_end();
    return 0;
}