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

#ifndef _APLUS_UI_H
#define _APLUS_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief The default socket, on /tmp because unix_bind() needs a filesystem that stores an S_IFSOCK dirent.
 */
#define UI_DEFAULT_SOCKET "/tmp/aplus-wm.sock"

#define UI_PROTOCOL_VERSION 7
#define UI_TITLE_MAX        64

/**
 * @brief The oldest version a server still accepts, echoing it back in its UI_EV_HELLO.
 */
#define UI_PROTOCOL_VERSION_MIN 6

/**
 * @brief How long an icon name may be, terminator included; a longer one resolves to nothing.
 */
#define UI_ICON_NAME_MAX 128

/**
 * @brief How far the server rounds a window's corners off, the same for every window it draws.
 *
 * Here rather than in the server because a client painting its own edge has to trace the
 * curve the server clips it to, or its border stops short of the corners.
 */
#define UI_WINDOW_RADIUS 20


/**
 * @brief The wire protocol: every message carries its own length, since AF_UNIX is SOCK_STREAM only here.
 *
 * The socket carries control only; a window's pixels live in a shared memory segment both ends map.
 */

#define UI_REQ_HELLO          0x0001
#define UI_REQ_CREATE_WINDOW  0x0002
#define UI_REQ_COMMIT         0x0003
#define UI_REQ_SET_TITLE      0x0004
#define UI_REQ_DESTROY_WINDOW 0x0005

/**
 * @brief Asks for a size, which the server grants by sending a UI_EV_CONFIGURE the way a resize drag does.
 *
 * A server that has never heard of this request drains it and leaves the window the size it
 * was created at.
 */
#define UI_REQ_RESIZE_WINDOW 0x0006

/**
 * @brief Names the icon the server draws in a window's titlebar, out of the server's own icon theme.
 *
 * A server that has never heard of this request drains it and draws no icon.
 */
#define UI_REQ_SET_ICON 0x0007

/**
 * @brief Names the shape the server gives the pointer while it is over a window's content area.
 *
 * A server that has never heard of this request drains it and keeps its arrow.
 */
#define UI_REQ_SET_CURSOR 0x0008

/**
 * @brief Shows a context menu at a point in a window's content area, which the server draws, places and puts away.
 *
 * The answer is a UI_EV_MENU carrying the same serial. A server that has never heard of this
 * request drains it and shows nothing, so no answer comes.
 */
#define UI_REQ_POPUP_MENU 0x0009

#define UI_EV_HELLO     0x8001
#define UI_EV_CONFIGURE 0x8002
#define UI_EV_KEY       0x8003
#define UI_EV_POINTER   0x8004
#define UI_EV_FOCUS     0x8005
#define UI_EV_CLOSE     0x8006

/**
 * @brief Sent when the pointer stops being over a window's content area; arriving needs no event of its own.
 */
#define UI_EV_LEAVE 0x8007

/**
 * @brief A wheel step, kept apart from UI_EV_POINTER because that one goes out on every motion.
 */
#define UI_EV_SCROLL 0x8008

/**
 * @brief The theme a client draws with, sent right after UI_EV_HELLO and again whenever it changes.
 *
 * Only to a client that said hello with UI_EV_THEME_SINCE or later.
 */
#define UI_EV_THEME       0x8009
#define UI_EV_THEME_SINCE 7

/**
 * @brief The answer to a UI_REQ_POPUP_MENU: the id of the item picked, or 0 when the menu went away without one.
 */
#define UI_EV_MENU 0x800A


/**
 * @brief The largest payload a peer may name, which is what stops a length from becoming an arbitrary allocation.
 */
#define UI_MSG_PAYLOAD_MAX (32 * 1024)


typedef struct {

    uint16_t type;
    uint16_t flags;
    uint32_t length;

} __attribute__((packed)) ui_msg_header_t;


typedef struct {

    uint32_t version;

} __attribute__((packed)) ui_msg_hello_t;


/**
 * @brief What a window asks to be, named once at creation and fixed for the rest of its life.
 *
 * Its size is not: see UI_REQ_RESIZE_WINDOW.
 */

#define UI_WINDOW_DECORATED 0

/**
 * @brief No titlebar, no border, no close button: the surface is the whole window.
 *
 * The server still stacks, focuses and shadows it, but owns none of its pixels, so it can
 * neither be dragged by a titlebar nor resized by an edge -- see ui_window_create_ex(). Its
 * corners are rounded off by UI_WINDOW_RADIUS like any other window's.
 */
#define UI_WINDOW_BORDERLESS (1 << 0)

/**
 * @brief Placed in the middle of the display rather than on the cascade.
 *
 * The frame is what is centred, decorations included, and it happens as the window is
 * created: there is no request to move one, and a window placed after the fact would be
 * seen somewhere else first.
 */
#define UI_WINDOW_CENTERED (1 << 1)

/**
 * @brief The surface carries alpha, and the server blends it over the desktop and the windows below.
 *
 * It is premultiplied CAIRO_FORMAT_ARGB32 rather than RGB24 -- the same stride and the same
 * segment size, so nothing about the configure changes, only what the fourth byte means.
 * A creation flag because the format has to be settled before the segment exists, which is
 * before a client could have said anything about it.
 */
#define UI_WINDOW_TRANSLUCENT (1 << 2)

/**
 * @brief Slid out of the taskbar's rail as a drawer, borderless, over a background the server paints.
 */
#define UI_WINDOW_DRAWER (1 << 3)

/**
 * @brief Every flag this version knows, which is what both ends validate against.
 */
#define UI_WINDOW_FLAGS_ALL (UI_WINDOW_BORDERLESS | UI_WINDOW_CENTERED | UI_WINDOW_TRANSLUCENT | UI_WINDOW_DRAWER)


typedef struct {

    uint16_t width;
    uint16_t height;

    uint32_t flags;

    char title[UI_TITLE_MAX];

} __attribute__((packed)) ui_msg_create_window_t;


/**
 * @brief A commit carries no pixels, only the rectangle the client changed in the surface both ends map.
 */

typedef struct {

    uint32_t window_id;

    uint32_t serial;

    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;

} __attribute__((packed)) ui_msg_commit_t;


typedef struct {

    uint32_t window_id;
    char title[UI_TITLE_MAX];

} __attribute__((packed)) ui_msg_set_title_t;


/**
 * @brief An icon by theme name, empty for the server's generic one.
 */
typedef struct {

    uint32_t window_id;
    char icon[UI_ICON_NAME_MAX];

} __attribute__((packed)) ui_msg_set_icon_t;


/**
 * @brief The pointer shapes a window can ask for over its content area.
 */
typedef enum {

    UI_CURSOR_DEFAULT = 0,
    UI_CURSOR_TEXT,

    UI_CURSOR_COUNT,

} ui_cursor_t;


/**
 * @brief A pointer shape, as one of ui_cursor_t; one the server does not know is its arrow.
 */
typedef struct {

    uint32_t window_id;
    uint8_t cursor;

} __attribute__((packed)) ui_msg_set_cursor_t;


typedef struct {

    uint32_t window_id;
    uint16_t width;
    uint16_t height;

} __attribute__((packed)) ui_msg_resize_t;


/**
 * @brief How many items a context menu holds, and how long an item's label may be, terminator included.
 */
#define UI_MENU_ITEMS_MAX 16
#define UI_MENU_LABEL_MAX 48

/**
 * @brief What a context menu item is besides a label: a rule between groups, greyed out, something destructive,
 * or something asked about first.
 *
 * A UI_MENU_CONFIRM item is not picked by the first press: the menu stays out and the item reads as its confirm
 * text, its label and a question mark when that is empty, and only a second press on it picks it.
 */
#define UI_MENU_SEPARATOR (1 << 0)
#define UI_MENU_DISABLED  (1 << 1)
#define UI_MENU_DANGER    (1 << 2)
#define UI_MENU_CONFIRM   (1 << 3)

/**
 * @brief One item of a context menu, its id never 0, which is what a dismissed menu answers with.
 */
typedef struct {

    uint32_t id;
    uint32_t flags;

    char label[UI_MENU_LABEL_MAX];
    char icon[UI_ICON_NAME_MAX];
    char confirm[UI_MENU_LABEL_MAX];

} __attribute__((packed)) ui_menu_item_t;


/**
 * @brief A context menu, top to bottom, opened at a point in a window's content area.
 */
typedef struct {

    uint32_t window_id;
    uint32_t serial;

    int16_t x;
    int16_t y;

    uint16_t count;

    ui_menu_item_t items[UI_MENU_ITEMS_MAX];

} __attribute__((packed)) ui_msg_popup_menu_t;


typedef struct {

    uint32_t window_id;
    uint32_t serial;
    uint32_t id;

} __attribute__((packed)) ui_msg_menu_t;


typedef struct {

    uint32_t window_id;

} __attribute__((packed)) ui_msg_window_t;


/**
 * @brief A configure hands over a surface as well as a size, the segment being created and owned by the server.
 */

typedef struct {

    uint32_t window_id;
    uint32_t serial;
    uint16_t width;
    uint16_t height;

    int32_t shm_id;

    uint32_t stride;

    uint32_t shm_size;

} __attribute__((packed)) ui_msg_configure_t;


typedef struct {

    uint32_t window_id;

    uint16_t vkey;
    uint8_t down;

} __attribute__((packed)) ui_msg_key_t;


typedef struct {

    uint32_t window_id;

    int16_t x;
    int16_t y;
    uint8_t buttons;

} __attribute__((packed)) ui_msg_pointer_t;


typedef struct {

    uint32_t window_id;
    uint8_t focused;

} __attribute__((packed)) ui_msg_focus_t;


/**
 * @brief A wheel step in detents, positive being away from the user, which scrolls a view towards its start.
 */

typedef struct {

    uint32_t window_id;

    int16_t dx;
    int16_t dy;

} __attribute__((packed)) ui_msg_scroll_t;


/**
 * @brief How long a theme's name, a path inside a theme and a variant's name may be, terminator included.
 */
#define UI_THEME_NAME_MAX    32
#define UI_THEME_PATH_MAX    128
#define UI_THEME_VARIANT_MAX 16

/**
 * @brief The variants a theme may come in.
 */
#define UI_THEME_VARIANT_DARK  "dark"
#define UI_THEME_VARIANT_LIGHT "light"


/**
 * @brief A colour on the wire, straight rather than premultiplied, each component 0 to 1.
 */
typedef struct {

    double r;
    double g;
    double b;
    double a;

} __attribute__((packed)) ui_msg_color_t;


/**
 * @brief Every role of the theme clients draw their widgets with, as the server resolved it.
 */
typedef struct {

    char name[UI_THEME_NAME_MAX];

    ui_msg_color_t background;
    ui_msg_color_t surface;
    ui_msg_color_t surface_sunken;
    ui_msg_color_t primary;
    ui_msg_color_t on_primary;
    ui_msg_color_t secondary;
    ui_msg_color_t on_secondary;
    ui_msg_color_t danger;
    ui_msg_color_t on_danger;
    ui_msg_color_t hover;
    ui_msg_color_t active;
    ui_msg_color_t border;
    ui_msg_color_t focus_ring;
    ui_msg_color_t text;
    ui_msg_color_t text_muted;

    double disabled_fade;
    double corner_radius;
    double font_size;

    char font_regular[UI_THEME_PATH_MAX];
    char font_medium[UI_THEME_PATH_MAX];
    char font_bold[UI_THEME_PATH_MAX];

    char font_mono[UI_THEME_PATH_MAX];
    char font_mono_bold[UI_THEME_PATH_MAX];
    double font_mono_size;

    char variant[UI_THEME_VARIANT_MAX];

} __attribute__((packed)) ui_msg_theme_t;


#define UI_BUTTON_LEFT   (1 << 0)
#define UI_BUTTON_RIGHT  (1 << 1)
#define UI_BUTTON_MIDDLE (1 << 2)


/**
 * @brief A rectangle, in whatever space the caller is working in.
 */

typedef struct {

    int x;
    int y;
    int width;
    int height;

} ui_rect_t;


/**
 * @brief How many damaged rectangles are kept apart before they start being merged.
 */
#define UI_DAMAGE_MAX 8


/**
 * @brief What still has to be repainted, as a few rectangles rather than the box around them.
 */
typedef struct {

    ui_rect_t rects[UI_DAMAGE_MAX];
    size_t count;

} ui_damage_t;


void ui_damage_reset(ui_damage_t* damage);
void ui_damage_add(ui_damage_t* damage, ui_rect_t rect);
void ui_damage_clip(ui_damage_t* damage, int width, int height);


/**
 * @brief Client API.
 */

typedef struct ui_connection ui_connection_t;
typedef struct ui_window ui_window_t;


typedef enum {

    UI_EVENT_NONE = 0,
    UI_EVENT_KEY,
    UI_EVENT_POINTER,
    UI_EVENT_CONFIGURE,
    UI_EVENT_FOCUS,
    UI_EVENT_CLOSE,
    UI_EVENT_LEAVE,
    UI_EVENT_SCROLL,
    UI_EVENT_THEME,
    UI_EVENT_MENU,

} ui_event_type_t;


typedef struct {

    ui_event_type_t type;
    uint32_t window_id;

    union {

        struct {
            uint16_t vkey;
            uint8_t down;
        } key;

        struct {
            int16_t x;
            int16_t y;
            uint8_t buttons;
        } pointer;

        struct {
            uint16_t width;
            uint16_t height;
            uint32_t serial;
        } configure;

        struct {
            uint8_t focused;
        } focus;

        struct {
            int16_t dx;
            int16_t dy;
        } scroll;

        struct {
            uint32_t serial;
            uint32_t id;
        } menu;
    };

} ui_event_t;


/**
 * @brief Connects to the server and takes up the theme it announces, before anything is drawn.
 *
 * @param path The socket to connect to, or NULL for UI_DEFAULT_SOCKET.
 * @param retry_ms How long to keep retrying for, which lets a client start alongside the server.
 * @return The connection, or NULL.
 */
ui_connection_t* ui_connect(const char* path, int retry_ms);
void ui_disconnect(ui_connection_t* conn);
int ui_connection_fd(ui_connection_t* conn);

ui_window_t* ui_window_create(ui_connection_t* conn, int width, int height, const char* title);
ui_window_t* ui_window_create_ex(ui_connection_t* conn, int width, int height, const char* title, uint32_t flags);
void ui_window_destroy(ui_window_t* win);

/**
 * @brief Reports the window's back buffer, which ui_window_commit() copies to the server, valid until the next ui_window_apply_configure().
 */
uint32_t* ui_window_pixels(ui_window_t* win);
int ui_window_width(ui_window_t* win);
int ui_window_height(ui_window_t* win);
size_t ui_window_stride(ui_window_t* win);
uint32_t ui_window_id(ui_window_t* win);
uint32_t ui_window_flags(ui_window_t* win);

/**
 * @brief Reports whether the window was created with UI_WINDOW_TRANSLUCENT.
 *
 * Cairo writes ARGB32 premultiplied, so anything drawing through ui_window_pixels() by hand
 * into such a window has to as well.
 *
 * @param win The window.
 * @return true when its surface carries alpha.
 */
bool ui_window_translucent(ui_window_t* win);

/**
 * @brief Asks the server for a size, which arrives as a UI_EV_CONFIGURE rather than taking effect here.
 *
 * Asking is not getting: the server clamps what it is given and may answer with another
 * size or, if nothing changed, with nothing at all. The window keeps the size it has until
 * ui_window_apply_configure() takes the answer up.
 *
 * @param win The window.
 * @param width The width asked for.
 * @param height The height asked for.
 * @return 0, or -1 with errno set.
 */
int ui_window_request_size(ui_window_t* win, int width, int height);

/**
 * @brief Adopts the surface the last UI_EV_CONFIGURE announced, along with its size and serial.
 *
 * Call it from wherever drawing is serialised, never from the event loop if that is another thread.
 *
 * @param win The window to configure.
 * @return 1 if the surface changed, 0 if nothing was pending, -1 on error.
 */
int ui_window_apply_configure(ui_window_t* win);

void ui_window_damage(ui_window_t* win, int x, int y, int width, int height);
void ui_window_damage_all(ui_window_t* win);
int ui_window_commit(ui_window_t* win);
int ui_window_set_title(ui_window_t* win, const char* title);

/**
 * @brief Names the icon the server draws in the window's titlebar, which starts as the program's own name.
 *
 * @param win The window.
 * @param name A name in the icon theme, or NULL for the server's generic icon.
 * @return 0, or -1 with errno set: ENAMETOOLONG for a name UI_ICON_NAME_MAX cannot hold.
 */
int ui_window_set_icon(ui_window_t* win, const char* name);

/**
 * @brief Sets the pointer shape over the window's content area, which starts as the server's arrow.
 *
 * @param win The window.
 * @param cursor The shape.
 * @return 0, or -1 with errno set: EINVAL for a shape ui_cursor_t does not name.
 */
int ui_window_set_cursor(ui_window_t* win, ui_cursor_t cursor);

/**
 * @brief Asks the server to show a context menu, whose answer arrives later as a UI_EVENT_MENU with the same serial.
 *
 * @param win The window the menu belongs to.
 * @param x The point it opens at, in the content area.
 * @param y The point it opens at, in the content area.
 * @param items Its items, top to bottom, each with a non-zero id.
 * @param count How many there are, at most UI_MENU_ITEMS_MAX.
 * @param serial Receives the serial the answer carries, or NULL.
 * @return 0, or -1 with errno set.
 */
int ui_window_popup_menu(ui_window_t* win, int x, int y, const ui_menu_item_t* items, size_t count, uint32_t* serial);

/**
 * @brief Waits for the next event on a connection.
 *
 * @param conn The connection to read from.
 * @param out Receives the event.
 * @param timeout_ms Blocks when negative, polls at 0, waits that long when positive.
 * @return 1 when an event was stored, 0 on timeout, -1 on error.
 */
int ui_next_event(ui_connection_t* conn, ui_event_t* out, int timeout_ms);


/**
 * @brief Framing helpers, shared with the server so that both ends agree byte for byte.
 */

ssize_t ui_send_all(int fd, const void* buf, size_t size);
ssize_t ui_recv_all(int fd, void* buf, size_t size);
int ui_send_msg(int fd, uint16_t type, const void* payload, size_t size);


#ifdef __cplusplus
}
#endif

#endif
