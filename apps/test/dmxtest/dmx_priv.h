#ifndef _DMX_PRIV_H
#define _DMX_PRIV_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>


typedef struct {
    double x;
    double y;

    struct {
        double margin_left;
        double margin_top;
        double margin_right;
        double margin_bottom;
        double border_width;
        double border_radius;
        
        double w;
        double h;
        
        cairo_surface_t* surface;
        cairo_t* cr;

        list(dmx_object_t*, childs);
    } container;
} dmx_object_t;



#define DMX_OBJECT(x)               ((dmx_object_t*) (x))
#define DMX_CONTAINER(x)            (DMX_WINDOW(x)->container)


#define DMX_ERROR(a...)                                             \
    {                                                               \
        fprintf(stderr, "dmx-error: %s\n", strerror(errno));        \
                                                                    \
        if(a)                                                       \
            fprintf(stderr, a);                                     \
            fprintf(stderr, "\n");                                  \
        }                                                           \
                                                                    \
        exit(1);                                                    \
    }


#define DMX_VALIDATE_PTR(x)                                         \
    {                                                               \
        if(unlikely(!(x))) {                                        \
            errno = EINVAL;                                         \
            DMX_ERROR("Null Pointer: %s", #x);                      \
        }                                                           \
    }





void dmx_object_new(dmx_widget_t*);
void dmx_object_destroy(dmx_widget_t);
void dmx_object_move(dmx_widget_t, double, double);


void dmx_container_set_margin(dmx_widget_t, double, double, double, double);
void dmx_container_get_margin(dmx_widget_t, double*, double*, double*, double*);
void dmx_container_set_border_width(dmx_widget_t, double);
void dmx_container_get_border_width(dmx_widget_t, double*);
void dmx_container_set_border_radius(dmx_widget_t, double);
void dmx_container_get_border_radius(dmx_widget_t, double*);
void dmx_container_set_size(dmx_widget_t, double, double);
void dmx_container_get_size(dmx_widget_t, double*, double*);

void dmx_container_add(dmx_widget_t, dmx_widget_t);
void dmx_container_remove(dmx_widget_t, dmx_widget_t);


#endif