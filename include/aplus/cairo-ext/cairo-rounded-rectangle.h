#ifndef _CAIRO_ROUNDED_RECTANGLE_H
#define _CAIRO_ROUNDED_RECTANGLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cairo/cairo.h>




/* void cairo_rounded_rectangle(cr, x, y, w, h, r)
    cairo_t* cr,
    double x,
    double y,
    double w,
    double h,
    double r,   // Radius
*/

#define cairo_rounded_rectangle(cr, x, y, w, h, r) {                        \
    cairo_new_sub_path(cr);                                                 \
    cairo_arc(cr, x + w - r, y + r, r, -(M_PI / 2.0), 0);                   \
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2.0);                  \
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);                   \
    cairo_arc(cr, x + r, y + r, r, M_PI, M_PI * 1.5);                       \
    cairo_close_path(cr);                                                   \
}
    
#ifdef __cplusplus
}
#endif
#endif