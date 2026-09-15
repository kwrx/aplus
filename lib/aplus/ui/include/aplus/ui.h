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

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif


//? The socket has to live on a filesystem that can store an S_IFSOCK dirent, which
//? rules out the ext2 root: unix_bind() creates a real directory entry. /tmp is the
//? tmpfs mounted by /etc/fstab.
#define UI_DEFAULT_SOCKET "/tmp/aplus-wm.sock"

#define UI_PROTOCOL_VERSION 2
#define UI_TITLE_MAX        64


//* Wire protocol.
//*
//* AF_UNIX is SOCK_STREAM only here -- there is no SOCK_SEQPACKET -- so every message
//* carries its own length and both sides have to reassemble frames by hand.
//*
//* The socket carries control only. A window's pixels live in a System V shared memory
//* segment that both ends map, so the largest thing that ever goes down the socket is a
//* window title.

#define UI_REQ_HELLO          0x0001
#define UI_REQ_CREATE_WINDOW  0x0002
#define UI_REQ_COMMIT         0x0003
#define UI_REQ_SET_TITLE      0x0004
#define UI_REQ_DESTROY_WINDOW 0x0005

#define UI_EV_HELLO     0x8001
#define UI_EV_CONFIGURE 0x8002
#define UI_EV_KEY       0x8003
#define UI_EV_POINTER   0x8004
#define UI_EV_FOCUS     0x8005
#define UI_EV_CLOSE     0x8006

//? Sent when the pointer stops being over a window's content area. There is no matching
//? "enter": arriving somewhere is already described by the UI_EV_POINTER that follows the
//? pointer in. Leaving is the one transition that produces no event of its own, and
//? without it a client that highlights whatever is under the pointer keeps the last thing
//? it highlighted lit forever.
#define UI_EV_LEAVE 0x8007


//? Nothing in this protocol is large any more -- the biggest message is a title -- but a
//? peer could still name any length it liked, and the cap is what stops that from becoming
//? an arbitrary allocation.
#define UI_MSG_PAYLOAD_MAX (32 * 1024)


typedef struct {

    uint16_t type;
    uint16_t flags;
    uint32_t length;

} __attribute__((packed)) ui_msg_header_t;


typedef struct {

    uint32_t version;

} __attribute__((packed)) ui_msg_hello_t;


typedef struct {

    uint16_t width;
    uint16_t height;
    char title[UI_TITLE_MAX];

} __attribute__((packed)) ui_msg_create_window_t;


//* A commit carries no pixels. Both ends have the window's surface mapped, so the client has
//* already written them; what the server is being told is which rectangle changed.

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

} __attribute__((packed)) ui_msg_window_t;


//* A configure hands over a surface as well as a size. The segment is created and owned by
//* the server, which is what lets it go on compositing a window whose client has died, and
//* removes it (shmctl(2) IPC_RMID) as soon as the window is gone or has been resized: a
//* removed segment stays alive until its last holder detaches, so a client still drawing into
//* the previous one is never pulled out from under.

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


#define UI_BUTTON_LEFT   (1 << 0)
#define UI_BUTTON_RIGHT  (1 << 1)
#define UI_BUTTON_MIDDLE (1 << 2)


//* Client API.

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
    };

} ui_event_t;


//? path may be NULL for UI_DEFAULT_SOCKET. retry_ms > 0 keeps retrying for that long,
//? which is what lets a client be started from the same script as the server without
//? a sleep in between.
ui_connection_t* ui_connect(const char* path, int retry_ms);
void ui_disconnect(ui_connection_t* conn);
int ui_connection_fd(ui_connection_t* conn);

ui_window_t* ui_window_create(ui_connection_t* conn, int width, int height, const char* title);
void ui_window_destroy(ui_window_t* win);

//? The window's surface, shared with the server: what is written here is what the server
//? composites, with no copy in between. A commit is therefore only a statement about which
//? rectangle changed, and the pointer is valid until the next ui_window_apply_configure().
uint32_t* ui_window_pixels(ui_window_t* win);
int ui_window_width(ui_window_t* win);
int ui_window_height(ui_window_t* win);
size_t ui_window_stride(ui_window_t* win);
uint32_t ui_window_id(ui_window_t* win);

//? Adopt the surface the last UI_EVENT_CONFIGURE announced, along with its size and serial.
//? Deliberately not done inside ui_next_event(): the event loop and the drawing code are
//? usually different threads, and swapping the pixel buffer out from under a draw would be a
//? use-after-free. Call it from wherever drawing is serialised. Returns 1 if the surface
//? changed, 0 if nothing was pending, -1 on error. Until it is called the window keeps the
//? surface it has -- the server has removed that segment but cannot destroy it while this
//? client still holds it -- and commits stamped with the old serial are dropped rather than
//? read against the wrong memory.
int ui_window_apply_configure(ui_window_t* win);

void ui_window_damage(ui_window_t* win, int x, int y, int width, int height);
void ui_window_damage_all(ui_window_t* win);
int ui_window_commit(ui_window_t* win);
int ui_window_set_title(ui_window_t* win, const char* title);

//? timeout_ms < 0 blocks, 0 polls, > 0 waits. Returns 1 when an event was stored,
//? 0 on timeout, -1 on error (including a server that went away).
int ui_next_event(ui_connection_t* conn, ui_event_t* out, int timeout_ms);


//* Framing helpers, shared with the server so that both ends agree byte for byte.

ssize_t ui_send_all(int fd, const void* buf, size_t size);
ssize_t ui_recv_all(int fd, void* buf, size_t size);
int ui_send_msg(int fd, uint16_t type, const void* payload, size_t size);


#ifdef __cplusplus
}
#endif

#endif
