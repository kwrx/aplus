#include <iostream>
#include <cmath>
#include <ptk/Window.h>


using namespace std;
using namespace ptk;


double WindowStyle::MarginLeft = 5.0;
double WindowStyle::MarginTop = 32.0;
double WindowStyle::MarginRight = 10.0;
double WindowStyle::MarginBottom = 37.0;
double WindowStyle::BorderRadius = 10.0;

void WindowStyle::DrawBorders(Window* W) {
    if(!W->View)
        return;

    cairo_t* cr = W->View->Fx;
    cairo_save(cr);
    cairo_new_sub_path(cr);
    cairo_arc(cr, W->Width - WindowStyle::BorderRadius, WindowStyle::BorderRadius, WindowStyle::BorderRadius, -90.0 * (M_PI / 180.0), 0.0);
    cairo_arc(cr, W->Width, W->Height, 0, 0, 90 * (M_PI / 180.0));
    cairo_arc(cr, 0, W->Height, 0, 90.0 * (M_PI / 180.0), M_PI);
    cairo_arc(cr, WindowStyle::BorderRadius, WindowStyle::BorderRadius, WindowStyle::BorderRadius, M_PI, 270 * (M_PI / 180.0));
    cairo_close_path(cr);

    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.3, 0.3, 0.3, 0.5);
    cairo_set_line_width(cr, 5.0);
    cairo_stroke(cr);

    cairo_set_font_face(cr, Font::Load("Ubuntu Regular"));
    cairo_set_font_size(cr, 14.0);
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_move_to(cr, WindowStyle::MarginLeft + 10.0, 20.0);
    cairo_show_text(cr, W->Title.c_str());
    cairo_restore(cr);
}