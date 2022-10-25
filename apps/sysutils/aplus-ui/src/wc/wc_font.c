#include <wc/wc.h>
#include <wc/wc_font.h>
#include <wc/wc_display.h>
#include <wc/wc_renderer.h>
#include <wc/wc_cursor.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

static FT_Library ft = NULL;
static wc_fontface_t* queue = NULL;


int wc_font_initialize() {
    
    if(FT_Init_FreeType(&ft) != 0)
        return errno = EIO, -1;


    if(access("/usr/share/fonts", R_OK) != 0)
        return -1;

    if(access("/etc/fonts", R_OK) != 0)
        return -1;

    if(access("/etc/fonts/fonts.conf", R_OK) != 0)
        return -1;


    
    FILE* fp = fopen("/etc/fonts/fonts.conf", "r");

    if(!fp) {
        return errno = EIO, -1;
    }


    do {

        char buffer[BUFSIZ] = { 0 };

        if(!fgets(buffer, BUFSIZ, fp)) {
            break;
        }


        if(buffer[0] == '\0') {
            continue;
        }

        if(buffer[0] == '#') {
            continue;
        }

        if(buffer[strlen(buffer) - 1] == '\n') {
            buffer[strlen(buffer) - 1] = '\0';
        }


        char* face   = strtok(buffer, ":");
        char* family = strtok(NULL, ":");
        char* weight = strtok(NULL, ":");
        char* slant  = strtok(NULL, ":");


        if(!face || !family || !weight || !slant) {
            continue;
        }


        wc_fontface_t* e = calloc(1, sizeof(wc_fontface_t));

        if(!e) {
            return errno = ENOMEM, -1;
        }


        e->next = queue;

        strncpy(e->path, face, sizeof(e->path) - 1);
        strncpy(e->family, family, sizeof(e->family) - 1);


        if(0) { }
#if defined(CAIRO_FONT_WEIGHT_LIGHT)
        else if(strcmp(weight, "Light") == 0) {
            e->weight = CAIRO_FONT_WEIGHT_LIGHT;
        }
#endif
#if defined(CAIRO_FONT_WEIGHT_MEDIUM)
        else if(strcmp(weight, "Medium") == 0) {
            e->weight = CAIRO_FONT_WEIGHT_MEDIUM;
        }
#endif
#if defined(CAIRO_FONT_WEIGHT_BOLD)
        else if(strcmp(weight, "Bold") == 0) {
            e->weight = CAIRO_FONT_WEIGHT_BOLD;
        }
#endif
#if defined(CAIRO_FONT_WEIGHT_NORMAL)
        else {
            e->weight = CAIRO_FONT_WEIGHT_NORMAL;
        }
#endif


        if(0) { }
#if defined(CAIRO_FONT_SLANT_ITALIC)
        else if(strcmp(slant, "Italic") == 0) {
            e->slant = CAIRO_FONT_SLANT_ITALIC;
        }
#endif
#if defined(CAIRO_FONT_SLANT_OBLIQUE)
        else if(strcmp(slant, "Oblique") == 0) {
            e->slant = CAIRO_FONT_SLANT_OBLIQUE;
        }
#endif
#if defined(CAIRO_FONT_SLANT_NORMAL)
        else {
            e->slant = CAIRO_FONT_SLANT_NORMAL;
        }
#endif


        queue = e;


        LOG("Found Face '%s' Family '%s' Weight '%s' Slant '%s'\n", e->path, e->family, weight, slant);

    } while(feof(fp) == 0);
    

    LOG("font subsystem initialized\n");

    return 0;

}


int wc_font_from_family(struct wc_font** font, const char* family) {

    assert(font);
    assert(family);


    for(wc_fontface_t* i = queue; i; i = i->next) {

        if(strcmp(i->family, family) == 0) {

            if((*font = calloc(1, sizeof(struct wc_font))) == NULL) {
                return errno = ENOMEM, -1;
            }

            (*font)->slant  = i->slant;
            (*font)->weight = i->weight;


            FT_Face face = NULL;

            if(FT_New_Face(ft, i->path, 0, &face) != 0) {
                free(*font);
                return errno = EIO, -1;
            }

            if(((*font)->face = cairo_ft_font_face_create_for_ft_face(face, 0)) == NULL) {
                free(*font);
                return errno = EIO, -1;
            }

            wc_ref_init(&(*font)->ref, wc_font_destroy, *font);

            return 0;
        }
        
    }

    return errno = ENOENT, -1;

}


int wc_font_from_family_and_style(struct wc_font** font, const char* family, cairo_font_slant_t slant, cairo_font_weight_t weight) {

    assert(font);
    assert(family);


    for(wc_fontface_t* i = queue; i; i = i->next) {

        if(strcmp(i->family, family) == 0 && i->slant == slant && i->weight == weight) {

            if((*font = calloc(1, sizeof(struct wc_font))) == NULL) {
                return errno = ENOMEM, -1;
            }

            (*font)->slant  = i->slant;
            (*font)->weight = i->weight;
 

            FT_Face face = NULL;

            if(FT_New_Face(ft, i->path, 0, &face) != 0) {
                free(*font);
                return errno = EIO, -1;
            }

            if(((*font)->face = cairo_ft_font_face_create_for_ft_face(face, 0)) == NULL) {
                free(*font);
                return errno = EIO, -1;
            }

            wc_ref_init(&(*font)->ref, wc_font_destroy, *font);

            return 0;
        }
        
    }

    return errno = ENOENT, -1;

}


int wc_font_from_path(struct wc_font** font, const char* path) {

    assert(font);
    assert(path);
    

    if((*font = calloc(1, sizeof(struct wc_font))) == NULL) {
        return errno = ENOMEM, -1;
    }

    (*font)->slant  = CAIRO_FONT_SLANT_NORMAL;
    (*font)->weight = CAIRO_FONT_WEIGHT_NORMAL;


    FT_Face face = NULL;

    if(FT_New_Face(ft, path, 0, &face) != 0) {
        free(*font);
        return errno = EIO, -1;
    }

    if(((*font)->face = cairo_ft_font_face_create_for_ft_face(face, 0)) == NULL) {
        free(*font);
        return errno = EIO, -1;
    }


    wc_ref_init(&(*font)->ref, wc_font_destroy, *font);

    return 0;

}


int wc_font_destroy(struct wc_font* font) {

    assert(font);

    if(font->face) {
        cairo_font_face_destroy(font->face);
    }

    free(font);

    return 0;

}





