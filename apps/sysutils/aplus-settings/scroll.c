/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2026 Antonino Natale
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

#include <string.h>

#include "settings.h"


#define SETTINGS_SCROLL_STEP   48
#define SETTINGS_THUMB_IDLE    4
#define SETTINGS_THUMB_ACTIVE  6
#define SETTINGS_THUMB_MIN     24
#define SETTINGS_THUMB_ALPHA   0.4
#define SETTINGS_THUMB_LIT     0.7
#define SETTINGS_TRACK_PAGE    0.9


void settings_scroller_init(settings_scroller_t* s, ui_widget_t* thumb, ui_widget_t* anchor, void (*changed)(void)) {

    memset(s, 0, sizeof(*s));

    s->thumb   = thumb;
    s->anchor  = anchor;
    s->changed = changed;
    s->px      = -1;
    s->py      = -1;
}


static int settings_scroller_range(const settings_scroller_t* s) {
    return s->content > s->view.height ? s->content - s->view.height : 0;
}


bool settings_scroller_scrollable(const settings_scroller_t* s) {
    return settings_scroller_range(s) > 0 && s->view.height > 0;
}


static ui_rect_t settings_scroller_thumb_rect(const settings_scroller_t* s) {

    const int track = s->view.height;
    const int range = settings_scroller_range(s);

    int length = (int)((long)track * s->view.height / (s->content > 0 ? s->content : 1));

    length = length < SETTINGS_THUMB_MIN ? SETTINGS_THUMB_MIN : length;
    length = length > track ? track : length;

    const int top   = s->view.y + (range > 0 ? (int)((long)(track - length) * s->offset / range) : 0);
    const int width = s->hovered || s->dragging ? SETTINGS_THUMB_ACTIVE : SETTINGS_THUMB_IDLE;

    return (ui_rect_t){s->track_x + (SETTINGS_SCROLL_TRACK - width) / 2, top, width, length};
}


static void settings_scroller_paint(settings_scroller_t* s) {

    const bool shown = settings_scroller_scrollable(s) && (!s->anchor || ui_widget_visible(s->anchor));

    if (shown) {

        ui_color_t color = ui_theme()->text_muted;

        color.a *= s->hovered || s->dragging ? SETTINGS_THUMB_LIT : SETTINGS_THUMB_ALPHA;

        const ui_rect_t r = settings_scroller_thumb_rect(s);

        ui_panel_set_color(s->thumb, color);
        ui_panel_set_radius(s->thumb, r.width / 2.0);
        ui_widget_place(s->thumb, r);
    }

    ui_widget_set_visible(s->thumb, shown);
}


static bool settings_scroller_move(settings_scroller_t* s, int offset) {

    const int range = settings_scroller_range(s);

    offset = offset > range ? range : offset;
    offset = offset < 0 ? 0 : offset;

    if (offset == s->offset) {
        return false;
    }

    s->offset = offset;

    if (s->changed) {
        s->changed();
    }

    settings_scroller_paint(s);

    return true;
}


bool settings_scroller_set(settings_scroller_t* s, ui_rect_t view, int track_x, int content) {

    s->view    = view;
    s->track_x = track_x;
    s->content = content;

    const int range = settings_scroller_range(s);
    const int clamp = s->offset > range ? range : s->offset;

    const bool moved = clamp != s->offset;

    s->offset = clamp;

    settings_scroller_paint(s);

    return moved;
}


static bool settings_scroller_over(const settings_scroller_t* s, int x, int y, int right) {
    return x >= s->view.x && x < right && y >= s->view.y && y < s->view.y + s->view.height;
}


static bool settings_scroller_on_track(const settings_scroller_t* s, int x, int y) {
    return settings_scroller_scrollable(s) && x >= s->track_x && x < s->track_x + SETTINGS_SCROLL_TRACK && y >= s->view.y && y < s->view.y + s->view.height;
}


bool settings_scroller_pointer(settings_scroller_t* s, const ui_event_t* event) {

    if (event->type == UI_EVENT_LEAVE) {

        const bool lit = s->hovered || s->dragging;

        s->px       = -1;
        s->py       = -1;
        s->hovered  = false;
        s->dragging = false;
        s->buttons  = 0;

        if (lit) {
            settings_scroller_paint(s);
        }

        return false;
    }

    if (event->type != UI_EVENT_POINTER) {
        return false;
    }


    const int x          = event->pointer.x;
    const int y          = event->pointer.y;
    const bool down      = (event->pointer.buttons & UI_BUTTON_LEFT) != 0;
    const bool pressed   = down && !(s->buttons & UI_BUTTON_LEFT);
    const bool was_lit   = s->hovered || s->dragging;
    const ui_rect_t knob = settings_scroller_thumb_rect(s);

    s->px      = x;
    s->py      = y;
    s->buttons = event->pointer.buttons;

    bool handled = false;

    if (s->dragging) {

        if (down) {

            const int travel = s->view.height - knob.height;
            const int top    = y - s->grab - s->view.y;

            if (travel > 0) {
                settings_scroller_move(s, (int)((long)top * settings_scroller_range(s) / travel));
            }

        } else {
            s->dragging = false;
        }

        handled = true;

    } else if (pressed && settings_scroller_on_track(s, x, y)) {

        if (y >= knob.y && y < knob.y + knob.height) {

            s->dragging = true;
            s->grab     = y - knob.y;

        } else {

            const int page = (int)(s->view.height * SETTINGS_TRACK_PAGE);

            settings_scroller_move(s, s->offset + (y < knob.y ? -page : page));
        }

        handled = true;
    }

    s->hovered = settings_scroller_on_track(s, x, y);

    if (was_lit != (s->hovered || s->dragging)) {
        settings_scroller_paint(s);
    }

    return handled;
}


bool settings_scroller_wheel(settings_scroller_t* s, const ui_event_t* event) {

    if (event->type != UI_EVENT_SCROLL || !settings_scroller_over(s, s->px, s->py, s->track_x + SETTINGS_SCROLL_TRACK)) {
        return false;
    }

    settings_scroller_move(s, s->offset - event->scroll.dy * SETTINGS_SCROLL_STEP);

    return true;
}
