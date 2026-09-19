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


/**
 * @brief Widgets, layered on top of the raw surface a ui_window_t hands out.
 *
 * Nothing here names a cairo type, and an app that outgrows them still has ui_window_pixels().
 */


/**
 * @brief A colour, as components rather than a packed pixel, since blending and dimming are arithmetic.
 */

typedef struct {

    double r;
    double g;
    double b;
    double a;

} ui_color_t;


/**
 * @brief Colour literals, spelled as brace lists so that they can also initialise a theme at file scope.
 */
#define UI_RGB(r, g, b)     {(r) / 255.0, (g) / 255.0, (b) / 255.0, 1.0}
#define UI_RGBA(r, g, b, a) {(r) / 255.0, (g) / 255.0, (b) / 255.0, (a)}

ui_color_t ui_rgb(uint8_t r, uint8_t g, uint8_t b);
ui_color_t ui_rgba(uint8_t r, uint8_t g, uint8_t b, double a);

/**
 * @brief Composites one colour onto another, both straight, which is how the hover and active roles apply.
 *
 * @param under The colour underneath.
 * @param over The translucent wash to put over it.
 * @return The result.
 */
ui_color_t ui_color_blend(ui_color_t under, ui_color_t over);


/**
 * @brief The colour scheme, as roles rather than colours, so that a second theme is this struct filled in once.
 */

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

/**
 * @brief Sets the theme a view is created against, or restores the dark default when given NULL.
 *
 * A view takes its copy when it is created, so set this before ui_view_create().
 */
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


typedef struct ui_view ui_view_t;
typedef struct ui_widget ui_widget_t;


typedef void (*ui_action_fn)(ui_widget_t* widget, void* user);

/**
 * @brief Called with the row a list selection or activation lands on, or -1 when nothing is selected.
 */
typedef void (*ui_list_fn)(ui_widget_t* widget, int index, void* user);

/**
 * @brief Called once when the view is created and again after every configure, with the content size in place.
 */
typedef void (*ui_layout_fn)(ui_view_t* view, int width, int height, void* user);

/**
 * @brief Called for a key event; return true to say the key was handled.
 */
typedef bool (*ui_key_fn)(ui_view_t* view, uint16_t vkey, bool down, void* user);


/**
 * @brief The view: it owns the widgets, the drawing surface over the window's pixels, and what has changed.
 */

ui_view_t* ui_view_create(ui_window_t* window);
void ui_view_destroy(ui_view_t* view);

ui_window_t* ui_view_window(ui_view_t* view);

void ui_view_on_layout(ui_view_t* view, ui_layout_fn fn, void* user);
void ui_view_on_key(ui_view_t* view, ui_key_fn fn, void* user);

/**
 * @brief Feeds one event into the widgets, applying a configure here so the caller need not.
 *
 * @param view The view to dispatch into.
 * @param event The event to act on.
 * @return true when the view made something of it.
 */
bool ui_view_dispatch(ui_view_t* view, const ui_event_t* event);

/**
 * @brief Reports whether the client has been told to close, by the titlebar button or otherwise.
 */
bool ui_view_closed(ui_view_t* view);

void ui_view_invalidate(ui_view_t* view);
bool ui_view_needs_paint(ui_view_t* view);

/**
 * @brief Repaints whatever is dirty and commits it.
 *
 * @param view The view to present.
 * @return 1 if a frame went out, 0 if nothing needed painting, -1 on error.
 */
int ui_view_present(ui_view_t* view);

/**
 * @brief Gives one widget the keyboard, taking it from whatever held it.
 *
 * Keys reach the focused widget first, and only fall through to ui_view_on_key() when it declines them.
 * A click moves the focus by itself, but only onto a widget that takes keys at all, never onto a button.
 *
 * @param view The view to move the focus in.
 * @param widget The widget to focus, or NULL to focus nothing.
 * @return Whether anything changed.
 */
bool ui_view_focus(ui_view_t* view, ui_widget_t* widget);
ui_widget_t* ui_view_focused(const ui_view_t* view);


/**
 * @brief Runs the event loop until the window closes or the server goes away.
 *
 * @param view The view to run.
 * @return 0 on a clean close, -1 otherwise.
 */
int ui_view_run(ui_view_t* view);


/**
 * @brief Widgets belong to the view and are freed with it; a pointer stays valid until it is removed.
 */

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


/**
 * @brief Panel: a filled rectangle, and the only widget that is purely background and takes no pointer input.
 */

ui_widget_t* ui_panel_create(ui_view_t* view);
void ui_panel_set_color(ui_widget_t* widget, ui_color_t color);
void ui_panel_set_radius(ui_widget_t* widget, double radius);
void ui_panel_set_border(ui_widget_t* widget, ui_color_t color, double width);


/**
 * @brief Label: one line of text, clipped to its rect and transparent behind.
 */

ui_widget_t* ui_label_create(ui_view_t* view, const char* text);
void ui_label_set_text(ui_widget_t* widget, const char* text);
const char* ui_label_text(const ui_widget_t* widget);
void ui_label_set_align(ui_widget_t* widget, ui_align_t align);
void ui_label_set_color(ui_widget_t* widget, ui_color_t color);
void ui_label_set_font(ui_widget_t* widget, ui_font_weight_t weight, double size);
void ui_label_set_padding(ui_widget_t* widget, int padding);


/**
 * @brief Button.
 */

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

/**
 * @brief Runs the click action as if the button had been pressed, for binding a key to a button.
 */
void ui_button_activate(ui_widget_t* widget);

/**
 * @brief Draws the button as held without running anything, to make a keyboard shortcut visible.
 */
void ui_button_set_held(ui_widget_t* widget, bool held);
bool ui_button_held(const ui_widget_t* widget);


/**
 * @brief List: one selectable row per item, scrolled by the wheel, the arrow keys or its own scrollbar.
 *
 * A row is a name and an optional detail drawn against the right edge, which is what a size column is.
 */

ui_widget_t* ui_list_create(ui_view_t* view);

void ui_list_clear(ui_widget_t* widget);

/**
 * @brief Appends a row, copying both strings.
 *
 * @param widget The list to append to.
 * @param text The name, ellipsised when it does not fit.
 * @param detail The right-hand column, or NULL for none.
 * @param user Carried along with the row and handed back by ui_list_item_user().
 * @return The index of the new row, or -1 on error.
 */
int ui_list_add(ui_widget_t* widget, const char* text, const char* detail, void* user);

size_t ui_list_count(const ui_widget_t* widget);
const char* ui_list_text(const ui_widget_t* widget, int index);
void* ui_list_item_user(const ui_widget_t* widget, int index);

int ui_list_selected(const ui_widget_t* widget);

/**
 * @brief Selects a row and scrolls it into view, or clears the selection when given -1.
 */
void ui_list_select(ui_widget_t* widget, int index);
void ui_list_scroll_to(ui_widget_t* widget, int index);

/**
 * @brief Sets the height of a row in pixels, or restores the height the theme implies when given 0.
 */
void ui_list_set_row_height(ui_widget_t* widget, int height);

void ui_list_on_select(ui_widget_t* widget, ui_list_fn fn, void* user);

/**
 * @brief Sets what a double click or Enter on a row runs.
 */
void ui_list_on_activate(ui_widget_t* widget, ui_list_fn fn, void* user);


/**
 * @brief Grid: cell rectangles over a region, whose rows and columns share out the remainder pixels.
 */

typedef struct {

    ui_rect_t bounds;

    int columns;
    int rows;

    int column_gap;
    int row_gap;

} ui_grid_t;


ui_grid_t ui_grid(ui_rect_t bounds, int columns, int rows, int gap);
ui_rect_t ui_grid_cell(const ui_grid_t* grid, int column, int row, int colspan, int rowspan);

/**
 * @brief Shrinks a rectangle by an inset on every side, clamped at empty.
 */
ui_rect_t ui_rect_inset(ui_rect_t rect, int inset);


#ifdef __cplusplus
}
#endif

#endif
