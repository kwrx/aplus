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

#define UI_PROTOCOL_VERSION 6
#define UI_TITLE_MAX        64

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
 * @brief Every flag this version knows, which is what both ends validate against.
 */
#define UI_WINDOW_FLAGS_ALL (UI_WINDOW_BORDERLESS | UI_WINDOW_CENTERED | UI_WINDOW_TRANSLUCENT)


typedef struct {

    uint16_t width;
    uint16_t height;

    //? UI_WINDOW_*. Unknown bits are refused rather than ignored, so that a client asking
    //? for something this server has never heard of fails at once instead of silently
    //? getting a window that is not what it asked for.
    uint32_t flags;

    char title[UI_TITLE_MAX];

} __attribute__((packed)) ui_msg_create_window_t;


/**
 * @brief A commit carries no pixels, only the rectangle the client changed in the surface both ends map.
 */

typedef struct {

    uint32_t window_id;

    //? Echoed back from the last UI_EV_CONFIGURE the client drew against. The server
    //? drops a commit whose serial is stale, which is what keeps a resize that races an
    //? in-flight frame from being read against the wrong surface.
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


typedef struct {

    uint32_t window_id;
    uint16_t width;
    uint16_t height;

} __attribute__((packed)) ui_msg_resize_t;


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

    //? System V shared memory id of the window's surface, to be passed to shmat(2).
    int32_t shm_id;

    //? Bytes per row. Not width * 4: cairo picks the stride for an image surface, and the
    //? two ends have to agree on it byte for byte since they are writing and reading the
    //? same memory.
    uint32_t stride;

    //? Size of the segment, so the client can sanity check what it attached.
    uint32_t shm_size;

} __attribute__((packed)) ui_msg_configure_t;


typedef struct {

    uint32_t window_id;

    //? Raw KEY_* code from <aplus/input.h>, exactly as it came out of /dev/kbd. The
    //? server does not own a keymap: translation stays wherever the characters are
    //? actually needed.
    uint16_t vkey;
    uint8_t down;

} __attribute__((packed)) ui_msg_key_t;


typedef struct {

    uint32_t window_id;

    //? Relative to the content area, which excludes the server-drawn decorations.
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
    };

} ui_event_t;


/**
 * @brief Connects to the server.
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
 * @brief Reports the window's surface, shared with the server, valid until the next ui_window_apply_configure().
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
