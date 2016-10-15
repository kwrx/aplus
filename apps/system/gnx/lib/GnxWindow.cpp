#include <iostream>
#include <list>
#include <aplus/gnx.h>

#include "../context.h"

using namespace std;



GnxWindow::GnxWindow(GnxWindow* Parent) {
    this->Parent = Parent;
    this->Childrens = new list<GnxWindow*>();
    this->dirtyRects = new list<GnxRect*>();
    
    this->Background = &GnxColors::WindowBackground;
    this->Foreground = &GnxColors::WindowForeground;
}

GnxWindow::GnxWindow(GnxWindow* Parent, int Width, int Height) : GnxWindow(Parent) {
    this->Width = Width;
    this->Height = Height;
    
    CTX_NEW(this, Width, Height);
}

GnxWindow::GnxWindow(GnxWindow* Parent, int Width, int Height, void* context) : GnxWindow(Parent) {
    this->Width = Width;
    this->Height = Height;
    
    this->Context = context;
}

GnxWindow::~GnxWindow() {
    /* TODO */
}

void GnxWindow::Paint() {
    //CTX_PAINT_WINDOW(this);
    
    cairo_save(CTX(this)->cx);
    
    cairo_set_source_rgba (
        CTX(this)->cx, 
        this->Background->R,
        this->Background->G,
        this->Background->B,
        this->Background->A
    );
    
    cairo_fill(CTX(this)->cx);
    cairo_paint(CTX(this)->cx);
    
    for(auto i = this->Childrens->begin(); i != this->Childrens->end(); i++) {
        /* TODO */
    }
    
    cairo_restore(CTX(this)->cx);
}