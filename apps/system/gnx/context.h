#ifndef _CONTEXT_H
#define _CONTEXT_H

#include <cairo/cairo.h>

class GnxContext {
    public:
        cairo_t* cx;
        cairo_surface_t* surface;
};


#define CTX(W)                                                                                          \
    ((GnxContext*) (W)->Context)

#define CTX_NEW(X, W, H)                                                                                \
    {                                                                                                   \
        X->Context = new GnxContext();                                                                  \
        CTX(X)->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);                        \
        CTX(X)->cx = cairo_create(CTX(X)->surface);                                                     \
    }
    
#define CTX_NEW_FROM_DATA(X, P, W, H)                                                                   \
    {                                                                                                   \
        X->Context = new GnxContext();                                                                  \
        CTX(X)->surface = cairo_image_surface_create_for_data(P, CAIRO_FORMAT_ARGB32, W, H, W * 4);     \
        CTX(X)->cx = cairo_create(CTX(X)->surface);                                                     \
    }
    

#endif