#include <iostream>
#include <aplus/base.h>
#include <aplus/fbdev.h>
#include <aplus/gnx.h>
#include <aplus/kmem.h>

#include <cairo/cairo.h>

using namespace std;
using namespace Gnx;


void Window::_init(Control* p, string t, int x, int y, int w, int h) {    
    this->Parent = p;
    this->Childs = new vector<Control*>();

    this->Text = t;
    this->X = x;
    this->Y = y;
    this->Width = w;
    this->Height = h;

    this->BackColor.A = 1.0;
    this->BackColor.R = 
    this->BackColor.G =
    this->BackColor.B = 0.7;

    if(!p) {
        this->gcontext = (struct __gnx_context*) kmem_alloc(sizeof(struct __gnx_context) + (w * h * LibGnx::BytesPerPixel)); /* FIXME: bpp */
        this->gcontext->x = x;
        this->gcontext->y = y;
        this->gcontext->w = w;
        this->gcontext->h = h;
        this->gcontext->dirty = 0;

        cairo_surface_t* surface = cairo_image_surface_create_for_data((unsigned char*) this->gcontext->frame, CAIRO_FORMAT_ARGB32, w, h, w * LibGnx::BytesPerPixel);
        this->graphics = new Graphics(cairo_create(surface), surface);

        LibGnx::AddContext((void*) this->gcontext);
    } else {
        this->graphics = NULL;
        this->gcontext = NULL;
    }
}

void Window::_fini() {
    delete this->Childs;

    if(!this->Parent) {
        cairo_surface_destroy(this->graphics->GetSurface<cairo_surface_t>());
        cairo_destroy(this->graphics->GetContext<cairo_t>());

        LibGnx::RemoveContext(this->gcontext);
    }
}

void Window::Paint(Graphics* G) {
    if(this->graphics)
        G = this->graphics;

    cairo_t* cr = G->GetContext<cairo_t>();
    cairo_surface_t* surface = G->GetSurface<cairo_surface_t>();

    cairo_save(cr);
    cairo_rectangle(cr, 0, 0, this->Width, this->Height);
    cairo_set_source_rgba(cr, this->BackColor.R, this->BackColor.G, this->BackColor.B, this->BackColor.A);
    cairo_fill(cr);
    cairo_restore(cr);

    for(auto i = this->Childs->begin(); i != this->Childs->end(); i++)
        (*i)->Paint(G);

    if(this->gcontext)
        this->gcontext->dirty = 1;
}