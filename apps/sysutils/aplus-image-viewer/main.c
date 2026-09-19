/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @brief An image viewer, whose picture is drawn by hand into a hole left between the widgets.
 *
 * The toolbar, the frame and the status line are widgets; everything inside the frame is cairo.
 */

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cairo/cairo.h>

#include <aplus/input.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>

#include <aplus/cairo-ext/cairo-jpeg.h>
#include <aplus/cairo-ext/cairo-webp.h>


#define VIEWER_NAME "aplus-image-viewer"

#define VIEWER_WINDOW_WIDTH  640
#define VIEWER_WINDOW_HEIGHT 480

#define VIEWER_MARGIN 12
#define VIEWER_GAP    8

#define VIEWER_TOOLBAR_HEIGHT 36
#define VIEWER_STATUS_HEIGHT  20

/**
 * @brief How far inside the frame the picture is drawn, so that the panel's border stays visible.
 */
#define VIEWER_FRAME_INSET 2

#define VIEWER_SCALE_MIN  0.05
#define VIEWER_SCALE_MAX  32.0
#define VIEWER_SCALE_STEP 1.25

/**
 * @brief Past this, the picture is drawn unsmoothed: at that magnification the pixels are the subject.
 */
#define VIEWER_SCALE_NEAREST 3.0

#define VIEWER_PAN_STEP 48


/**
 * @brief What a toolbar button does.
 */

typedef enum {

    VIEWER_ACTION_ZOOM_OUT = 0,
    VIEWER_ACTION_ZOOM_IN,
    VIEWER_ACTION_FIT,
    VIEWER_ACTION_ACTUAL,

    VIEWER_ACTION_COUNT,

} viewer_action_t;


/**
 * @brief The toolbar, laid out from the right edge in this order.
 */

static const struct {

    viewer_action_t action;

    const char* label;

    int width;

} viewer_buttons[] = {

    {VIEWER_ACTION_ZOOM_OUT, "\xE2\x88\x92", 36},
    {VIEWER_ACTION_ZOOM_IN,  "+",            36},
    {VIEWER_ACTION_FIT,      "Fit",          52},
    {VIEWER_ACTION_ACTUAL,   "1:1",          52},
};


/**
 * @brief Everything the viewer keeps between one event and the next.
 */

typedef struct {

    ui_connection_t* conn;
    ui_window_t* window;
    ui_view_t* view;

    ui_widget_t* toolbar;
    ui_widget_t* name;
    ui_widget_t* frame;
    ui_widget_t* status;
    ui_widget_t* buttons[VIEWER_ACTION_COUNT];

    //? Where the picture goes, in window coordinates. The view places the frame widget; the
    //? drawing below reads this and never asks a widget where it is.
    ui_rect_t area;

    //? The decoded picture, or NULL when nothing is loaded.
    cairo_surface_t* image;

    //? The whole placement: the picture's top-left corner in window coordinates, and how
    //? many window pixels one image pixel covers. Every gesture is a change to these two.
    double scale;
    double offset_x;
    double offset_y;

    //? Set while the scale is the frame's to choose, which is what makes a resize refit
    //? rather than leave a picture the user never zoomed sitting at a stale size.
    bool fit;

    bool panning;

    int pan_x;
    int pan_y;

    int pointer_x;
    int pointer_y;

    bool quit;

    char path[PATH_MAX];

} viewer_t;


static viewer_t viewer;


/**
 * @brief The rectangle the picture is drawn in, which is the frame minus the border the view draws.
 */

static ui_rect_t viewer_canvas(void) {

    return ui_rect_inset(viewer.area, VIEWER_FRAME_INSET);
}


static int viewer_image_width(void) {

    return viewer.image ? cairo_image_surface_get_width(viewer.image) : 0;
}


static int viewer_image_height(void) {

    return viewer.image ? cairo_image_surface_get_height(viewer.image) : 0;
}


/**
 * @brief Holds a scale inside the range the viewer works in.
 *
 * @param scale The scale to clamp.
 * @return The clamped scale.
 */

static double viewer_clamp_scale(double scale) {

    if (scale < VIEWER_SCALE_MIN) {
        return VIEWER_SCALE_MIN;
    }

    if (scale > VIEWER_SCALE_MAX) {
        return VIEWER_SCALE_MAX;
    }

    return scale;
}


/**
 * @brief Centres the picture along whichever axis it is smaller than the canvas on, and keeps
 *        its edges outside the canvas on the other.
 */

static void viewer_clamp_offset(void) {

    if (!viewer.image) {
        return;
    }


    const ui_rect_t canvas = viewer_canvas();

    const double width  = viewer_image_width() * viewer.scale;
    const double height = viewer_image_height() * viewer.scale;


    if (width <= canvas.width) {

        viewer.offset_x = canvas.x + (canvas.width - width) / 2.0;

    } else {

        if (viewer.offset_x > canvas.x) {
            viewer.offset_x = canvas.x;
        }

        if (viewer.offset_x + width < canvas.x + canvas.width) {
            viewer.offset_x = canvas.x + canvas.width - width;
        }
    }


    if (height <= canvas.height) {

        viewer.offset_y = canvas.y + (canvas.height - height) / 2.0;

    } else {

        if (viewer.offset_y > canvas.y) {
            viewer.offset_y = canvas.y;
        }

        if (viewer.offset_y + height < canvas.y + canvas.height) {
            viewer.offset_y = canvas.y + canvas.height - height;
        }
    }
}


/**
 * @brief The scale at which the whole picture fits the canvas, never enlarging one that already fits.
 *
 * @return The scale.
 */

static double viewer_fit_scale(void) {

    const ui_rect_t canvas = viewer_canvas();

    if (!viewer.image || canvas.width <= 0 || canvas.height <= 0) {
        return 1.0;
    }

    if (viewer_image_width() <= 0 || viewer_image_height() <= 0) {
        return 1.0;
    }


    const double sx = (double)canvas.width / (double)viewer_image_width();
    const double sy = (double)canvas.height / (double)viewer_image_height();

    double scale = sx < sy ? sx : sy;

    if (scale > 1.0) {
        scale = 1.0;
    }

    return viewer_clamp_scale(scale);
}


/**
 * @brief Fits the picture to the canvas and hands the scale back to the frame.
 */

static void viewer_refit(void) {

    viewer.scale = viewer_fit_scale();
    viewer.fit   = true;

    viewer_clamp_offset();
}


/**
 * @brief Rescales about a window point, which stays over the same pixel of the picture.
 *
 * @param scale The scale to go to, clamped to the viewer's range.
 * @param x The window x the gesture is anchored at.
 * @param y The window y the gesture is anchored at.
 */

static void viewer_zoom_at(double scale, double x, double y) {

    if (!viewer.image || viewer.scale <= 0.0) {
        return;
    }


    const double next = viewer_clamp_scale(scale);

    viewer.offset_x = x - (x - viewer.offset_x) * (next / viewer.scale);
    viewer.offset_y = y - (y - viewer.offset_y) * (next / viewer.scale);

    viewer.scale = next;
    viewer.fit   = false;

    viewer_clamp_offset();
}


/**
 * @brief Rescales about the middle of the canvas, which is what the toolbar and the keys do.
 *
 * @param scale The scale to go to.
 */

static void viewer_zoom_centered(double scale) {

    const ui_rect_t canvas = viewer_canvas();

    viewer_zoom_at(scale, canvas.x + canvas.width / 2.0, canvas.y + canvas.height / 2.0);
}


/**
 * @brief Moves the picture under the canvas by a number of window pixels.
 *
 * @param dx How far to move it horizontally.
 * @param dy How far to move it vertically.
 */

static void viewer_pan(int dx, int dy) {

    if (!viewer.image) {
        return;
    }

    viewer.offset_x += dx;
    viewer.offset_y += dy;

    viewer_clamp_offset();
}


/**
 * @brief Writes the size and the magnification into the status line.
 */

static void viewer_status(void) {

    if (!viewer.image) {

        ui_label_set_text(viewer.status, "no image");
        return;
    }


    char text[64];

    snprintf(text, sizeof(text), "%d x %d      %d%%", viewer_image_width(), viewer_image_height(), (int)(viewer.scale * 100.0 + 0.5));

    ui_label_set_text(viewer.status, text);
}


/**
 * @brief Decodes a png file into a picture the canvas can draw.
 *
 * @param path The file to read.
 * @return The surface, or NULL when the file is not an image this viewer reads.
 */

static cairo_surface_t* viewer_decode_png(const char* path) {

    cairo_surface_t* image = cairo_image_surface_create_from_png(path);

    if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {

        cairo_surface_destroy(image);
        return NULL;
    }

    return image;
}

/**
 * @brief Decodes a webp file into a picture the canvas can draw.
 *
 * @param path The file to read.
 * @return The surface, or NULL when the file is not an image this viewer reads.
 */

static cairo_surface_t* viewer_decode_webp(const char* path) {

    cairo_surface_t* image = cairo_image_surface_create_from_webp(path);

    if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {

        cairo_surface_destroy(image);
        return NULL;
    }

    return image;
}

/**
 * @brief Decodes a jpeg file into a picture the canvas can draw.
 *
 * @param path The file to read.
 * @return The surface, or NULL when the file is not an image this viewer reads.
 */

static cairo_surface_t* viewer_decode_jpeg(const char* path) {

    cairo_surface_t* image = cairo_image_surface_create_from_jpeg(path);

    if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {

        cairo_surface_destroy(image);
        return NULL;
    }

    return image;
}

/**
 * @brief Decodes a file into a picture the canvas can draw.
 *
 * @param path The file to read.
 * @return The surface, or NULL when the file is not an image this viewer reads.
 */

static cairo_surface_t* viewer_decode(const char* path) {

    const char* ext = strrchr(path, '.');

    if (!ext) {
        return NULL;
    }

    if (strcasecmp(ext, ".png") == 0) {
        return viewer_decode_png(path);
    }

    if (strcasecmp(ext, ".webp") == 0) {
        return viewer_decode_webp(path);
    }

    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) {
        return viewer_decode_jpeg(path);
    }

    return NULL;
}


/**
 * @brief Draws the loaded picture, already clipped to the frame and in window coordinates.
 *
 * @param cr The context over the window's pixels.
 * @param area The rectangle inside the frame the picture has to stay within.
 */

static void viewer_paint(cairo_t* cr, ui_rect_t area) {

    (void)area;

    if (!viewer.image) {
        return;
    }


    cairo_translate(cr, viewer.offset_x, viewer.offset_y);
    cairo_scale(cr, viewer.scale, viewer.scale);

    cairo_set_source_surface(cr, viewer.image, 0.0, 0.0);
    cairo_pattern_set_filter(cairo_get_source(cr), viewer.scale >= VIEWER_SCALE_NEAREST ? CAIRO_FILTER_NEAREST : CAIRO_FILTER_GOOD);

    cairo_paint(cr);
}


/**
 * @brief Repaints the canvas over the frame the view has just laid down, and damages it.
 */

static void viewer_draw(void) {

    if (viewer.area.width <= 0 || viewer.area.height <= 0) {
        return;
    }


    cairo_surface_t* surface = cairo_image_surface_create_for_data((unsigned char*)ui_window_pixels(viewer.window), CAIRO_FORMAT_RGB24, ui_window_width(viewer.window), ui_window_height(viewer.window), (int)ui_window_stride(viewer.window));

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return;
    }


    cairo_t* cr = cairo_create(surface);

    const ui_rect_t inner = viewer_canvas();

    cairo_rectangle(cr, inner.x, inner.y, inner.width, inner.height);
    cairo_clip(cr);

    viewer_paint(cr, inner);

    cairo_destroy(cr);

    cairo_surface_flush(surface);
    cairo_surface_destroy(surface);


    ui_window_damage(viewer.window, viewer.area.x, viewer.area.y, viewer.area.width, viewer.area.height);
}


/**
 * @brief Shows a file, replacing whatever was loaded before it.
 *
 * @param path The file to open.
 * @return 0 when the picture is on screen, -1 when the file could not be shown.
 */

static int viewer_open(const char* path) {

    cairo_surface_t* image = viewer_decode(path);

    if (viewer.image) {
        cairo_surface_destroy(viewer.image);
    }

    viewer.image = image;

    snprintf(viewer.path, sizeof(viewer.path), "%s", path);


    const char* name = strrchr(viewer.path, '/');

    name = name ? name + 1 : viewer.path;

    ui_label_set_text(viewer.name, name);


    char title[UI_TITLE_MAX];

    snprintf(title, sizeof(title), "%.40s - %s", name, VIEWER_NAME);

    ui_window_set_title(viewer.window, image ? title : VIEWER_NAME);


    if (image) {

        viewer_refit();
        viewer_status();

    } else {

        ui_label_set_text(viewer.status, "cannot show this file");
    }

    ui_view_invalidate(viewer.view);

    return image ? 0 : -1;
}


/**
 * @brief Runs a toolbar action.
 *
 * @param widget The button that was clicked.
 * @param user The action, carried as the button's user pointer.
 */

static void viewer_on_action(ui_widget_t* widget, void* user) {

    (void)widget;

    switch ((viewer_action_t)(intptr_t)user) {

        case VIEWER_ACTION_ZOOM_OUT:
            viewer_zoom_centered(viewer.scale / VIEWER_SCALE_STEP);
            break;

        case VIEWER_ACTION_ZOOM_IN:
            viewer_zoom_centered(viewer.scale * VIEWER_SCALE_STEP);
            break;

        case VIEWER_ACTION_FIT:
            viewer_refit();
            break;

        case VIEWER_ACTION_ACTUAL:
            viewer_zoom_centered(1.0);
            break;

        default:
            return;
    }

    viewer_status();
    ui_view_invalidate(viewer.view);
}


/**
 * @brief Reports whether a window point is inside the canvas.
 *
 * @param x The window x.
 * @param y The window y.
 * @return true when the point is over the picture's rectangle.
 */

static bool viewer_inside(int x, int y) {

    const ui_rect_t canvas = viewer_canvas();

    return x >= canvas.x && y >= canvas.y && x < canvas.x + canvas.width && y < canvas.y + canvas.height;
}


/**
 * @brief Drags the picture with the left button, the gesture only starting inside the canvas.
 *
 * @param event The pointer event.
 */

static void viewer_on_pointer(const ui_event_t* event) {

    const int x = event->pointer.x;
    const int y = event->pointer.y;

    viewer.pointer_x = x;
    viewer.pointer_y = y;


    if (!(event->pointer.buttons & UI_BUTTON_LEFT)) {

        viewer.panning = false;
        return;
    }


    if (!viewer.panning) {

        if (!viewer.image || !viewer_inside(x, y)) {
            return;
        }

        viewer.panning = true;
        viewer.pan_x   = x;
        viewer.pan_y   = y;

        return;
    }


    viewer_pan(x - viewer.pan_x, y - viewer.pan_y);

    viewer.pan_x = x;
    viewer.pan_y = y;

    ui_view_invalidate(viewer.view);
}


/**
 * @brief Zooms about the pointer, a detent at a time.
 *
 * @param event The scroll event.
 */

static void viewer_on_scroll(const ui_event_t* event) {

    if (!viewer.image || !viewer_inside(viewer.pointer_x, viewer.pointer_y)) {
        return;
    }


    double scale = viewer.scale;

    int steps = event->scroll.dy;

    for (; steps > 0; steps--) {
        scale *= VIEWER_SCALE_STEP;
    }

    for (; steps < 0; steps++) {
        scale /= VIEWER_SCALE_STEP;
    }


    viewer_zoom_at(scale, viewer.pointer_x, viewer.pointer_y);

    viewer_status();
    ui_view_invalidate(viewer.view);
}


/**
 * @brief Handles a key the widgets did not want.
 *
 * @param view The view the key arrived on.
 * @param vkey The KEY_* code.
 * @param down Whether the key went down or came up.
 * @param user Unused.
 * @return true when the key was acted on.
 */

static bool viewer_on_key(ui_view_t* view, uint16_t vkey, bool down, void* user) {

    (void)view;
    (void)user;

    if (!down) {
        return false;
    }


    switch (vkey) {

        case KEY_ESC:
        case KEY_Q:
            viewer.quit = true;
            return true;

        case KEY_EQUAL:
        case KEY_KPPLUS:
            viewer_zoom_centered(viewer.scale * VIEWER_SCALE_STEP);
            break;

        case KEY_MINUS:
        case KEY_KPMINUS:
            viewer_zoom_centered(viewer.scale / VIEWER_SCALE_STEP);
            break;

        case KEY_0:
        case KEY_F:
            viewer_refit();
            break;

        case KEY_1:
            viewer_zoom_centered(1.0);
            break;

        case KEY_LEFT:
            viewer_pan(VIEWER_PAN_STEP, 0);
            break;

        case KEY_RIGHT:
            viewer_pan(-VIEWER_PAN_STEP, 0);
            break;

        case KEY_UP:
            viewer_pan(0, VIEWER_PAN_STEP);
            break;

        case KEY_DOWN:
            viewer_pan(0, -VIEWER_PAN_STEP);
            break;

        default:
            return false;
    }


    viewer_status();
    ui_view_invalidate(viewer.view);

    return true;
}


/**
 * @brief Places the widgets, and records the rectangle the picture is drawn in.
 *
 * @param view Unused.
 * @param width The content width.
 * @param height The content height.
 * @param user Unused.
 */

static void viewer_layout(ui_view_t* view, int width, int height, void* user) {

    (void)view;
    (void)user;

    const int inner = width - VIEWER_MARGIN * 2;

    ui_rect_t toolbar = {VIEWER_MARGIN, VIEWER_MARGIN, inner > 0 ? inner : 0, VIEWER_TOOLBAR_HEIGHT};

    ui_widget_place(viewer.toolbar, toolbar);


    int x = toolbar.x + toolbar.width - 4;

    for (int i = (int)(sizeof(viewer_buttons) / sizeof(viewer_buttons[0])) - 1; i >= 0; i--) {

        ui_rect_t rect = {x - viewer_buttons[i].width, toolbar.y + 4, viewer_buttons[i].width, toolbar.height - 8};

        ui_widget_place(viewer.buttons[viewer_buttons[i].action], rect);

        x -= viewer_buttons[i].width + 4;
    }


    ui_rect_t name = {toolbar.x, toolbar.y, x - toolbar.x, toolbar.height};

    if (name.width < 0) {
        name.width = 0;
    }

    ui_widget_place(viewer.name, name);


    ui_rect_t status = {VIEWER_MARGIN, height - VIEWER_MARGIN - VIEWER_STATUS_HEIGHT, toolbar.width, VIEWER_STATUS_HEIGHT};

    ui_widget_place(viewer.status, status);


    const int frame_y = toolbar.y + toolbar.height + VIEWER_GAP;

    int frame_height = status.y - frame_y - VIEWER_GAP;

    if (frame_height < 0) {
        frame_height = 0;
    }

    ui_rect_t frame = {VIEWER_MARGIN, frame_y, toolbar.width, frame_height};

    ui_widget_place(viewer.frame, frame);

    viewer.area = frame;


    if (viewer.fit) {
        viewer_refit();
    } else {
        viewer_clamp_offset();
    }

    viewer_status();
}


/**
 * @brief Creates the widgets.
 *
 * @param view The view to create them in.
 * @return 0 on success, -1 when one of them could not be created.
 */

static int viewer_build(ui_view_t* view) {

    const ui_theme_t* theme = ui_theme();

    viewer.toolbar = ui_panel_create(view);
    viewer.name    = ui_label_create(view, VIEWER_NAME);
    viewer.frame   = ui_panel_create(view);
    viewer.status  = ui_label_create(view, "no image");

    if (!viewer.toolbar || !viewer.name || !viewer.frame || !viewer.status) {
        return -1;
    }


    for (size_t i = 0; i < sizeof(viewer_buttons) / sizeof(viewer_buttons[0]); i++) {

        ui_widget_t* button = ui_button_create(view, viewer_buttons[i].label, viewer_on_action, (void*)(intptr_t)viewer_buttons[i].action);

        if (!button) {
            return -1;
        }

        viewer.buttons[viewer_buttons[i].action] = button;
    }


    ui_panel_set_color(viewer.toolbar, theme->surface);
    ui_panel_set_radius(viewer.toolbar, theme->corner_radius);

    ui_panel_set_color(viewer.frame, theme->surface_sunken);
    ui_panel_set_radius(viewer.frame, theme->corner_radius);
    ui_panel_set_border(viewer.frame, theme->border, 1.0);

    ui_label_set_padding(viewer.name, 8);

    ui_label_set_color(viewer.status, theme->text_muted);
    ui_label_set_align(viewer.status, UI_ALIGN_RIGHT);

    return 0;
}


/**
 * @brief The event loop: ui_view_run() with the canvas painted over every frame the view sends out.
 *
 * @return 0 on a clean close, -1 otherwise.
 */

static int viewer_run(void) {

    while (!ui_view_closed(viewer.view) && !viewer.quit) {

        const int painted = ui_view_present(viewer.view);

        if (painted < 0) {
            return -1;
        }

        if (painted > 0) {

            viewer_draw();

            if (ui_window_commit(viewer.window) < 0) {
                return -1;
            }
        }


        ui_event_t event;

        const int e = ui_next_event(viewer.conn, &event, -1);

        if (e < 0) {
            return -1;
        }

        if (e == 0) {
            continue;
        }

        ui_view_dispatch(viewer.view, &event);


        switch (event.type) {

            case UI_EVENT_POINTER:
                viewer_on_pointer(&event);
                break;

            case UI_EVENT_SCROLL:
                viewer_on_scroll(&event);
                break;

            case UI_EVENT_LEAVE:
                viewer.panning = false;
                break;

            default:
                break;
        }
    }

    return 0;
}


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);

    viewer.scale = 1.0;
    viewer.fit   = true;


    viewer.conn = ui_connect(NULL, 5000);

    if (!viewer.conn) {
        fprintf(stderr, "%s: ui_connect() failed: %s\n", VIEWER_NAME, strerror(errno));
        return 1;
    }


    viewer.window = ui_window_create(viewer.conn, VIEWER_WINDOW_WIDTH, VIEWER_WINDOW_HEIGHT, VIEWER_NAME);

    if (!viewer.window) {
        fprintf(stderr, "%s: ui_window_create() failed: %s\n", VIEWER_NAME, strerror(errno));
        ui_disconnect(viewer.conn);
        return 1;
    }


    viewer.view = ui_view_create(viewer.window);

    if (!viewer.view) {
        fprintf(stderr, "%s: ui_view_create() failed: %s\n", VIEWER_NAME, strerror(errno));
        ui_window_destroy(viewer.window);
        ui_disconnect(viewer.conn);
        return 1;
    }


    if (viewer_build(viewer.view) < 0) {
        fprintf(stderr, "%s: cannot create the widgets\n", VIEWER_NAME);
        ui_view_destroy(viewer.view);
        ui_window_destroy(viewer.window);
        ui_disconnect(viewer.conn);
        return 1;
    }

    ui_view_on_key(viewer.view, viewer_on_key, NULL);
    ui_view_on_layout(viewer.view, viewer_layout, NULL);


    if (argc > 1 && viewer_open(argv[1]) < 0) {
        fprintf(stderr, "%s: cannot show %s\n", VIEWER_NAME, argv[1]);
    }


    const int status = viewer_run();

    if (status < 0) {
        fprintf(stderr, "%s: the connection went away: %s\n", VIEWER_NAME, strerror(errno));
    }

    if (viewer.image) {
        cairo_surface_destroy(viewer.image);
    }

    ui_view_destroy(viewer.view);
    ui_window_destroy(viewer.window);
    ui_disconnect(viewer.conn);

    return status < 0 ? 1 : 0;
}
