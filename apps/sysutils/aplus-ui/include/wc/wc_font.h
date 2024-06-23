#ifndef _WC_FONT_H
#define _WC_FONT_H

#include <cairo/cairo-ft.h>
#include <cairo/cairo.h>
#include <wc/wc.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct wc_font {

        cairo_font_slant_t slant;
        cairo_font_weight_t weight;

        cairo_font_face_t* face;

        wc_ref_t ref;

} wc_font_t;


typedef struct wc_fontface {

        char path[BUFSIZ];
        char family[BUFSIZ];

        cairo_font_slant_t slant;
        cairo_font_weight_t weight;

        struct wc_fontface* next;

} wc_fontface_t;


int wc_font_from_family(struct wc_font** font, const char* family);
int wc_font_from_family_and_style(struct wc_font** font, const char* family, cairo_font_slant_t slant, cairo_font_weight_t weight);
int wc_font_from_path(struct wc_font** font, const char* path);
int wc_font_destroy(struct wc_font* font);
int wc_font_initialize();

#ifdef __cplusplus
}
#endif

#endif
