#include <iostream>
#include <aplus/base.h>
#include <aplus/fbdev.h>
#include <aplus/gnx.h>

#include <cairo/cairo.h>

using namespace std;
using namespace Gnx;


void Control::_init(Control* p, string t, int x, int y, int w, int h) {
    this->Parent = p;
    this->Childs = new vector<Control*>();

    this->Text = t;
    this->X = x;
    this->Y = y;
    this->Width = w;
    this->Height = h;

    this->BackColor.R = 
    this->BackColor.G =
    this->BackColor.B = 0.7;
}

void Control::_fini() {
    delete this->Childs;
}

void Control::Paint(Graphics* G) {
    cairo_t* cr = G->GetContext<cairo_t>();
    cairo_surface_t* surface = G->GetSurface<cairo_surface_t>();

    cairo_save(cr);
    cairo_rectangle(cr, this->X, this->Y, this->Width, this->Height);
    cairo_set_source_rgba(cr, this->BackColor.R, this->BackColor.G, this->BackColor.B, this->BackColor.A);
    cairo_fill(cr);
    cairo_restore(cr);

    for(auto i = this->Childs->begin(); i != this->Childs->end(); i++)
        (*i)->Paint(G);
}

void Control::Move(double x, double y) {
    this->X = x;
    this->Y = y;
}