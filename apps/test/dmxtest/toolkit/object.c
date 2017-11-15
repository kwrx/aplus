#include "dmx_priv.h"

void dmx_object_new(dmx_widget_t* w) {
    (*w) = (dmx_widget_t) malloc(sizeof(dmx_object_t));
    if(!(*w))
        DMX_ERROR(NULL);
    
    memset(*w, 0, sizeof(dmx_object_t));
}

void dmx_object_destroy(dmx_widget_t w) {
    free(w);
}

void dmx_object_move(dmx_widget_t w, double x, double y) {
    DMX_VALIDATE_PTR(w);

    DMX_OBJECT(w)->x = x;
    DMX_OBJECT(w)->y = y;
}