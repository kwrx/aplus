#include "dmx_priv.h"


void dmx_container_set_margin(dmx_widget_t w, double l, double t, double r, double b) {
    DMX_VALIDATE_PTR(w);

    DMX_CONTAINER(w)->margin_left = l;
    DMX_CONTAINER(w)->margin_top = t;
    DMX_CONTAINER(w)->margin_right = r;
    DMX_CONTAINER(w)->margin_bottom = b;    
}

void dmx_container_get_margin(dmx_widget_t w, double* l, double* t, double* r, double* b) {
    DMX_VALIDATE_PTR(w && l && t && r && b);

    *l = DMX_CONTAINER(w)->margin_left;
    *t = DMX_CONTAINER(w)->margin_top;
    *r = DMX_CONTAINER(w)->margin_right;
    *b = DMX_CONTAINER(w)->margin_bottom;
}

void dmx_container_set_border_width(dmx_widget_t w, double b) {
    DMX_VALIDATE_PTR(w);

    DMX_CONTAINER(w)->border_width = b;
}

void dmx_container_get_border_width(dmx_widget_t w, double* b) {
    DMX_VALIDATE_PTR(w && b);
    
    *b = DMX_CONTAINER(w)->border_width;
}

void dmx_container_set_border_radius(dmx_widget_t w, double r) {
    DMX_VALIDATE_PTR(w);
    
    DMX_CONTAINER(w)->border_radius = r;
}

void dmx_container_get_border_radius(dmx_widget_t w, double* r) {
    DMX_VALIDATE_PTR(w && r);
    
    *r = DMX_CONTAINER(w)->border_radius;
}

void dmx_container_set_size(dmx_widget_t w, double x, double y) {
    DMX_VALIDATE_PTR(w);
    
    DMX_CONTAINER(w)->w = x;
    DMX_CONTAINER(w)->h = y;
}

void dmx_container_get_size(dmx_widget_t w, double* x, double* y) {
    DMX_VALIDATE_PTR(w);
    
    *x = DMX_CONTAINER(w)->w;
    *y = DMX_CONTAINER(w)->h;
}

void dmx_container_add(dmx_widget_t w, dmx_widget_t c) {
    DMX_VALIDATE_PTR(w & c);

    list_push(DMX_CONTAINER(w)->childs, c);
}

void dmx_container_remove(dmx_widget_t w, dmx_widget_t c) {
    DMX_VALIDATE_PTR(w & c);
    
    list_remove(DMX_CONTAINER(w)->childs, c);
}