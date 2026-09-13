/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
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

#ifndef _APLUS_UI_WIDGETS_H
#define _APLUS_UI_WIDGETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <aplus/ui.h>

#ifdef __cplusplus
extern "C" {
#endif


//* Widgets, layered on top of the raw surface a ui_window_t hands out.
//*
//* The window protocol deals in pixels and events; everything below turns those into a
//* list of things that draw themselves and say when they have changed. Cairo does the
//* drawing, but none of it appears here: an app that only needs the widgets in this
//* header never has to name a cairo type, and one that outgrows them still has
//* ui_window_pixels() to fall back on.


//* Colours are components rather than a packed pixel because everything downstream of
//* them -- blending a hover overlay, dimming a disabled control -- is arithmetic, and
//* doing that on 8-bit channels loses more than it saves.

typedef struct {

    double r;
    double g;
    double b;
    double a;

} ui_color_t;


//? Spelled as a brace list rather than a compound literal so that it can also initialise
//? a theme at file scope, where a compound literal is not a constant expression.
#define UI_RGB(r, g, b)     {(r) / 255.0, (g) / 255.0, (b) / 255.0, 1.0}
#define UI_RGBA(r, g, b, a) {(r) / 255.0, (g) / 255.0, (b) / 255.0, (a)}

ui_color_t ui_rgb(uint8_t r, uint8_t g, uint8_t b);
ui_color_t ui_rgba(uint8_t r, uint8_t g, uint8_t b, double a);

//? `over` composited onto `under`, both straight (non-premultiplied). This is how the
//? hover and active roles of a scheme are applied: one translucent wash that works over
//? whatever colour a control happens to have, instead of a second and third colour per
//? control.
ui_color_t ui_color_blend(ui_color_t under, ui_color_t over);


//* The colour scheme. Roles rather than colours: a widget asks for "the accent" or "the
//* text on a secondary surface" and the theme decides what that is, which is what makes a
//* second theme a matter of filling in this struct once.

typedef struct {

    //? Three depths, darkest to lightest is *not* the order: `background` is the window
    //? backdrop, `surface` sits raised on it, and `surface_sunken` is a well cut into it
    //? -- a display, a text area, anything that should read as recessed.
    ui_color_t background;
    ui_color_t surface;
    ui_color_t surface_sunken;

    //? The accent, for the one control on a screen that is the point of the screen. Paired
    //? with the colour text drawn on top of it has to be, since the pairing is a property
    //? of the scheme and not something a caller should have to get right.
    ui_color_t primary;
    ui_color_t on_primary;

    //? Ordinary controls.
    ui_color_t secondary;
    ui_color_t on_secondary;

    //? Destructive controls.
    ui_color_t danger;
    ui_color_t on_danger;

    //? State washes, composited over whatever a control is already painted with. Their
    //? alpha is what carries the strength, so the same two colours work on the accent and
    //? on a plain grey button alike.
    ui_color_t hover;
    ui_color_t active;

    ui_color_t border;
    ui_color_t focus_ring;

    ui_color_t text;
    ui_color_t text_muted;

    //? How much a disabled control fades towards the surface behind it, 0 to 1.
    double disabled_fade;

    double corner_radius;
    double font_size;

    //? Absolute paths. There is no fontconfig in the sysroot, so a family name would have
    //? nothing to resolve against.
    const char* font_regular;
    const char* font_bold;

} ui_theme_t;


const ui_theme_t* ui_theme_dark(void);

//? The theme a view is created against. Passing NULL restores the dark default. A view
//? takes its copy when it is created, so changing this does not restyle a window that
//? already exists -- set it before ui_view_create().
void ui_theme_set(const ui_theme_t* theme);
const ui_theme_t* ui_theme(void);


typedef enum {

    UI_FONT_REGULAR = 0,
    UI_FONT_BOLD,

} ui_font_weight_t;


typedef enum {

    UI_ALIGN_LEFT = 0,
    UI_ALIGN_CENTER,
    UI_ALIGN_RIGHT,

} ui_align_t;


typedef struct {

    int x;
    int y;
    int width;
    int height;

} ui_rect_t;


typedef struct ui_view ui_view_t;
typedef struct ui_widget ui_widget_t;


typedef void (*ui_action_fn)(ui_widget_t* widget, void* user);

//? Called once when the view is created and again after every configure the view has
//? applied, with the content size already in place. Everything positional belongs here:
//? a window that can be resized has no fixed geometry to hardcode anywhere else.
typedef void (*ui_layout_fn)(ui_view_t* view, int width, int height, void* user);

//? Return true to say the key was handled. Nothing in the view claims keys on its own, so
//? without a handler a window is pointer-only.
typedef bool (*ui_key_fn)(ui_view_t* view, uint16_t vkey, bool down, void* user);


//* The view. It owns the widgets, the drawing surface over the window's pixels, and the
//* record of what has changed since the last frame.

ui_view_t* ui_view_create(ui_window_t* window);
void ui_view_destroy(ui_view_t* view);

ui_window_t* ui_view_window(ui_view_t* view);

void ui_view_on_layout(ui_view_t* view, ui_layout_fn fn, void* user);
void ui_view_on_key(ui_view_t* view, ui_key_fn fn, void* user);

//? Feeds one event into the widgets. Returns true when the view made something of it,
//? which for a pointer event means a widget changed state. A configure is applied here,
//? surface and layout included, so a caller driving its own loop does not also have to
//? call ui_window_apply_configure().
bool ui_view_dispatch(ui_view_t* view, const ui_event_t* event);

//? Whether the client has been told to close, by the titlebar button or otherwise.
bool ui_view_closed(ui_view_t* view);

void ui_view_invalidate(ui_view_t* view);
bool ui_view_needs_paint(ui_view_t* view);

//? Repaints whatever is dirty and commits it. Returns 1 if a frame went out, 0 if
//? nothing needed painting, -1 on error.
int ui_view_present(ui_view_t* view);

//? Event loop: present, wait, dispatch, until the window closes or the server goes away.
//? Returns 0 on a clean close and -1 otherwise.
int ui_view_run(ui_view_t* view);


//* Widgets. They belong to the view and are freed with it; the returned pointer stays
//* valid until the view is destroyed or the widget is explicitly removed.

void ui_widget_destroy(ui_widget_t* widget);

void ui_widget_set_rect(ui_widget_t* widget, int x, int y, int width, int height);
void ui_widget_place(ui_widget_t* widget, ui_rect_t rect);
ui_rect_t ui_widget_rect(const ui_widget_t* widget);

void ui_widget_set_visible(ui_widget_t* widget, bool visible);
bool ui_widget_visible(const ui_widget_t* widget);

void ui_widget_set_enabled(ui_widget_t* widget, bool enabled);
bool ui_widget_enabled(const ui_widget_t* widget);

void ui_widget_set_user(ui_widget_t* widget, void* user);
void* ui_widget_user(const ui_widget_t* widget);

void ui_widget_invalidate(ui_widget_t* widget);


//* Panel: a filled rectangle, and the only widget that is purely background. It does not
//* take pointer input, so a widget sitting on one still gets its own events.

ui_widget_t* ui_panel_create(ui_view_t* view);
void ui_panel_set_color(ui_widget_t* widget, ui_color_t color);
void ui_panel_set_radius(ui_widget_t* widget, double radius);
void ui_panel_set_border(ui_widget_t* widget, ui_color_t color, double width);


//* Label: one line of text, clipped to its rect. Transparent, so whatever is behind it
//* shows through -- a label over a panel needs no colour of its own.

ui_widget_t* ui_label_create(ui_view_t* view, const char* text);
void ui_label_set_text(ui_widget_t* widget, const char* text);
const char* ui_label_text(const ui_widget_t* widget);
void ui_label_set_align(ui_widget_t* widget, ui_align_t align);
void ui_label_set_color(ui_widget_t* widget, ui_color_t color);
void ui_label_set_font(ui_widget_t* widget, ui_font_weight_t weight, double size);
void ui_label_set_padding(ui_widget_t* widget, int padding);


//* Button.

typedef enum {

    //? The scheme's secondary role: what a button looks like when nothing says otherwise.
    UI_BUTTON_STYLE_DEFAULT = 0,
    UI_BUTTON_STYLE_PRIMARY,
    UI_BUTTON_STYLE_DANGER,

} ui_button_style_t;


ui_widget_t* ui_button_create(ui_view_t* view, const char* text, ui_action_fn on_click, void* user);
void ui_button_set_text(ui_widget_t* widget, const char* text);
void ui_button_set_style(ui_widget_t* widget, ui_button_style_t style);
void ui_button_set_font(ui_widget_t* widget, ui_font_weight_t weight, double size);

//? Runs the click action as if the button had been pressed. For binding a key to a
//? button without duplicating what the button does.
void ui_button_activate(ui_widget_t* widget);

//? Draws the button as held without running anything. Pair it with a key going down and
//? up to make a keyboard shortcut visible on screen.
void ui_button_set_held(ui_widget_t* widget, bool held);
bool ui_button_held(const ui_widget_t* widget);


//* Grid: cell rectangles over a region, for a layout callback to place widgets with.
//* Rows and columns share out the remainder pixels, so the last cell ends flush with the
//* region rather than a few pixels short of it.

typedef struct {

    ui_rect_t bounds;

    int columns;
    int rows;

    int column_gap;
    int row_gap;

} ui_grid_t;


ui_grid_t ui_grid(ui_rect_t bounds, int columns, int rows, int gap);
ui_rect_t ui_grid_cell(const ui_grid_t* grid, int column, int row, int colspan, int rowspan);

//? `rect` shrunk by `inset` on every side, clamped at empty.
ui_rect_t ui_rect_inset(ui_rect_t rect, int inset);


#ifdef __cplusplus
}
#endif

#endif
