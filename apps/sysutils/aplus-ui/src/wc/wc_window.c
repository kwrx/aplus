#include <assert.h>
#include <cairo/cairo.h>
#include <errno.h>
#include <stdio.h>
#include <wc/wc.h>
#include <wc/wc_input.h>
#include <wc/wc_window.h>

#include <aplus/events.h>
#include <aplus/input.h>



static wc_window_t* queue = NULL;


void wc_window_enqueue(wc_window_t* window) {

    assert(window);

    window->next = queue;
    queue        = window;
}

void wc_window_dequeue(wc_window_t* window) {

    assert(window);

    wc_window_t* prev = NULL;
    wc_window_t* curr = queue;

    while (curr) {

        if (curr == window) {

            if (prev) {
                prev->next = curr->next;
            } else {
                queue = curr->next;
            }

            break;
        }

        prev = curr;
        curr = curr->next;
    }
}


int wc_window_create(wc_window_t** window, wc_window_t* parent) {

    assert(window);

    if (!(*window = calloc(1, sizeof(wc_window_t)))) {
        return errno = ENOMEM, -1;
    }

    wc_ref_init(&(*window)->ref, wc_window_destroy, *window);

    (*window)->parent = parent;
    (*window)->root   = parent ? parent->root : *window;

    (*window)->flags      = WC_WINDOW_FLAGS_NONE;
    (*window)->x          = 0;
    (*window)->y          = 0;
    (*window)->width      = 400;
    (*window)->height     = 300;
    (*window)->min_width  = 0;
    (*window)->min_height = 0;
    (*window)->max_width  = INT32_MAX;
    (*window)->max_height = INT32_MAX;


    wc_window_enqueue(*window);

    return 0;
}


int wc_window_destroy(wc_window_t* window) {

    assert(window);

    wc_window_dequeue(window);

    free(window);

    return 0;
}



void wc_window_set_title(wc_window_t* window, const char* title) {

    assert(window);
    assert(title);

    strncpy(window->title, title, WC_WINDOW_TITLE_MAX);

    window->flags |= WC_WINDOW_FLAGS_DIRTY;
}


void wc_window_set_position(wc_window_t* window, int x, int y) {

    assert(window);

    window->x = x;
    window->y = y;
}


void wc_window_set_size(wc_window_t* window, int width, int height) {

    assert(window);

    window->width  = width;
    window->height = height;

    window->flags |= WC_WINDOW_FLAGS_DIRTY;
}


void wc_window_set_min_size(wc_window_t* window, int width, int height) {

    assert(window);

    window->min_width  = width;
    window->min_height = height;
}


void wc_window_set_max_size(wc_window_t* window, int width, int height) {

    assert(window);

    window->max_width  = width;
    window->max_height = height;
}


void wc_window_set_flags(wc_window_t* window, int flags) {

    assert(window);

    window->flags = flags;
    window->flags |= WC_WINDOW_FLAGS_DIRTY;
}


void wc_window_set_font(wc_window_t* window, wc_font_t* font) {

    assert(window);
    assert(font);

    if (window->font) {
        wc_ref_dec(&window->font->ref);
    }

    wc_ref_inc(&font->ref);

    window->font = font;
    window->flags |= WC_WINDOW_FLAGS_DIRTY;
}


// void wc_window_receive_input(wc_window_t* window, event_t* ev) {

//     assert(window);
//     assert(ev);

//     switch(ev->ev_type) {

//         case EV_KEY:

//             if(ev->ev_key.vkey == BTN_LEFT) {



//             }

//             break;

//         default:
//             break;

//     }


// }


int wc_window_set_top(wc_window_t* window) {

    assert(window);

    wc_window_dequeue(window);
    wc_window_enqueue(window);

    return 0;
}

void wc_window_update(wc_window_t* window) {

    assert(window);

    if (wc_input_key_is_down(BTN_LEFT)) {

        int x = wc_input_cursor_x();
        int y = wc_input_cursor_y();

        if (wc_input_cursor_is_hover(window->x, window->y, window->width, window->height)) {

            wc_window_set_top(window);

            if (!(window->flags & WC_WINDOW_FLAGS_DRAG)) {

                window->flags |= WC_WINDOW_FLAGS_DRAG;
                window->drag_x = x - window->x;
                window->drag_y = y - window->y;
            }

            window->flags |= WC_WINDOW_FLAGS_FOCUSED;

        } else {

            window->flags &= ~WC_WINDOW_FLAGS_FOCUSED;
        }

        if (window->flags & WC_WINDOW_FLAGS_DRAG) {

            window->x = x - window->drag_x;
            window->y = y - window->drag_y;
        }

    } else {

        if (window->flags & WC_WINDOW_FLAGS_DRAG) {

            window->flags &= ~WC_WINDOW_FLAGS_DRAG;
            window->drag_x = 0;
            window->drag_y = 0;
        }
    }
}

void wc_window_draw(wc_window_t* window, wc_renderer_t* renderer) {

    assert(window);
    assert(renderer);


    cairo_t* cr = renderer->cr;


    cairo_save(cr);
    cairo_translate(cr, window->x, window->y);

    if (window->flags & WC_WINDOW_FLAGS_BORDERLESS) {

        cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 1.0);
        cairo_rectangle(cr, 0, 0, window->width, window->height);
        cairo_fill(cr);

        // TODO: Draw client area

    } else {

        cairo_set_source_rgba(cr, WC_WINDOW_COLOR_TITLEBAR_BG);
        cairo_rectangle(cr, 0, 0, window->width, WC_WINDOW_MEASURES_TITLEBAR_HEIGHT);
        cairo_fill(cr);


        if (window->title[0] && window->font) {

            cairo_set_source_rgba(cr, WC_WINDOW_COLOR_TITLEBAR_FG);
            cairo_move_to(cr, WC_WINDOW_MEASURES_TITLEBAR_OFFSET_X, WC_WINDOW_MEASURES_TITLEBAR_OFFSET_Y);
            cairo_set_font_face(cr, window->font->face);
            cairo_set_font_size(cr, WC_WINDOW_MEASURES_TITLEBAR_FONTSIZE);
            cairo_show_text(cr, window->title);
        }

        if (window->flags & WC_WINDOW_FLAGS_FOCUSED) {
            cairo_set_source_rgba(cr, WC_WINDOW_COLOR_BORDER_FOCUSED);
        } else {
            cairo_set_source_rgba(cr, WC_WINDOW_COLOR_BORDER);
        }

        cairo_rectangle(cr, 0, 0, window->width, window->height);
        cairo_stroke(cr);

        cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 1.0);
        cairo_rectangle(cr, 1, WC_WINDOW_MEASURES_TITLEBAR_HEIGHT + 1, window->width - 2, window->height - WC_WINDOW_MEASURES_TITLEBAR_HEIGHT - 2);
        cairo_fill(cr);

        // TODO: Draw client area
    }

    cairo_restore(cr);


    window->flags &= ~WC_WINDOW_FLAGS_DIRTY;
}


void wc_window_draw_all(wc_renderer_t* renderer) {

    assert(renderer);
    assert(renderer->cr);
    assert(renderer->surface);
    assert(renderer->display);


    cairo_save(renderer->cr);
    cairo_set_operator(renderer->cr, CAIRO_OPERATOR_SOURCE);

    for (wc_window_t* window = queue; window; window = window->next) {

        wc_window_update(window);
        wc_window_draw(window, renderer);

        cairo_rectangle(renderer->cr, 0, 0, renderer->display->var.xres, window->y);
        cairo_rectangle(renderer->cr, 0, window->y + window->height, renderer->display->var.xres, renderer->display->var.yres);
        cairo_rectangle(renderer->cr, 0, window->y, window->x, window->height);
        cairo_rectangle(renderer->cr, window->x + window->width, window->y, renderer->display->var.xres, window->height);
        cairo_clip(renderer->cr);
    }

    cairo_restore(renderer->cr);
}
