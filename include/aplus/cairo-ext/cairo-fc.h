#ifndef _CAIRO_EXT_FC_H
#define _CAIRO_EXT_FC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>

#define CAIRO_FONT_WEIGHT_LIGHT         ((cairo_font_weight_t) 128)
#define CAIRO_FONT_WEIGHT_MEDIUM        ((cairo_font_weight_t) 129)

int cairo_fc_load(const char*);
cairo_font_face_t* cairo_fc_font_face_create(const char*, cairo_font_slant_t, cairo_font_weight_t);
void cairo_fc_select_font_face(cairo_t*, const char*, cairo_font_slant_t, cairo_font_weight_t);
    
#ifdef __cplusplus
}
#endif
#endif