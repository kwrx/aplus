# Window and event API

Reference for `<aplus/ui.h>`, the surface layer. This is the whole of what a client needs to
put pixels on the screen; the widget layer is built on it and adds nothing private.

## Connection

```c
ui_connection_t* ui_connect(const char* path, int retry_ms);
void             ui_disconnect(ui_connection_t* conn);
int              ui_connection_fd(ui_connection_t* conn);
```

`path` may be `NULL` for `UI_DEFAULT_SOCKET`:

```c
#define UI_DEFAULT_SOCKET "/tmp/aplus-wm.sock"
```

The socket has to live on a filesystem that can store an `S_IFSOCK` dirent, which rules out
the ext2 root — `unix_bind()` creates a real directory entry. `/tmp` is the tmpfs mounted by
`/etc/fstab`.

`retry_ms` is how long to keep retrying a failed connect, in milliseconds, with a 50 ms
backoff. The server is usually started from the same script a moment earlier, so the socket
may not exist yet; retrying is what removes the need for a `sleep` in `init.sh`. Pass `0` to
fail immediately.

`ui_connect()` completes the version handshake before returning: it sends `UI_REQ_HELLO` and
waits for the reply, failing with `EPROTONOSUPPORT` if the server speaks a different
`UI_PROTOCOL_VERSION`. A connection you get back is one that agrees.

`ui_disconnect()` closes the socket and frees every window still attached, so an error path
can skip straight to it without destroying windows first.

`ui_connection_fd()` is the descriptor, for a client that polls it alongside its own. Read
it only through `ui_next_event()`: the stream is framed, and a stray `read()` desynchronises
it permanently.

## Window

```c
ui_window_t* ui_window_create(ui_connection_t* conn, int width, int height, const char* title);
void         ui_window_destroy(ui_window_t* win);

uint32_t* ui_window_pixels(ui_window_t* win);
int       ui_window_width(ui_window_t* win);
int       ui_window_height(ui_window_t* win);
size_t    ui_window_stride(ui_window_t* win);
uint32_t  ui_window_id(ui_window_t* win);

int ui_window_set_title(ui_window_t* win, const char* title);
```

`ui_window_create()` blocks until the server has configured the window, stepping over any
other message that arrives first. The size you asked for is a request — the server clamps to
at least 80×40 and to what fits — so read the real one back rather than assuming.

Titles are truncated at `UI_TITLE_MAX` (64) bytes including the terminator, in both the
constructor and `ui_window_set_title()`.

`ui_window_destroy()` tells the server and frees the buffer. The window id is the server's
handle for it and the field every event carries.

### The pixels

| | |
|---|---|
| Format | `0xFFRRGGBB` — 8 bits per channel, not premultiplied |
| Stride | `width * sizeof(uint32_t)`, always; no row padding |
| Origin | Top-left of the content area, inside the server's decorations |

The buffer belongs to the window and is replaced on resize, so hold the pointer no longer
than a frame. A resize that fits in the existing allocation reuses it rather than
reallocating: `sys_mmap()` never rewinds its cursor and `munmap()` does not give the address
space back, so a client that reallocated on every configure would eat its own mmap window
one drag at a time.

For a cairo context over these pixels, use `CAIRO_FORMAT_RGB24` — see
[tutorial-custom-drawing.md](tutorial-custom-drawing.md#your-own-cairo-context).

### Damage and commits

```c
void ui_window_damage(ui_window_t* win, int x, int y, int width, int height);
void ui_window_damage_all(ui_window_t* win);
int  ui_window_commit(ui_window_t* win);
```

Damage accumulates into one bounding rectangle, clipped to the window; several calls before
a commit merge into the box enclosing them all. Damaging two opposite corners therefore
sends the whole window, which is correct but not cheap — commit in between if the two regions
are far apart and the space between them has not changed.

`ui_window_commit()` sends the damaged region and clears it, returning `0` without sending
anything when there is no damage outstanding. A frame that is drawn but never committed is a
frame the server never hears about.

Large regions are split into bands automatically. The socket buffer is 65535 bytes and
writes only ever make partial progress, so a whole-surface commit could never fit in one
frame; each band names its own position and the server stitches them back together. The
caps are `UI_MSG_PAYLOAD_MAX` (32 KiB) and `UI_COMMIT_BAND_MAX` (16 KiB) — see
[protocol.md](protocol.md#commits-and-banding).

### Resizing

```c
int ui_window_apply_configure(ui_window_t* win);
```

This is the one piece of the surface API with a rule attached.

When a `UI_EVENT_CONFIGURE` arrives, `ui_next_event()` records the new size and serial but
changes nothing: reallocating the pixel buffer from the event path would free it under a
thread that is still drawing into it. The resize happens when you call
`ui_window_apply_configure()`, from wherever drawing is serialised.

| Return | |
|---|---|
| `1` | The surface changed — repaint everything |
| `0` | Nothing was pending |
| `-1` | Error; the configure stays pending so it can be retried |

Until it is called, the window keeps its old size and its old serial, and the server drops
every commit stamped with a stale serial. That is what makes a resize racing an in-flight
frame harmless rather than an overrun — and it is also why a client that never applies a
configure never paints again. This is the one event that cannot be ignored.

After a successful apply the buffer contents are gone: it is cleared if reused, fresh if
reallocated, and the whole surface is already marked damaged. Repaint everything.

A view does this for you inside `ui_view_dispatch()`.

## Events

```c
int ui_next_event(ui_connection_t* conn, ui_event_t* out, int timeout_ms);
```

| `timeout_ms` | |
|---|---|
| `< 0` | Block until an event arrives |
| `0` | Poll; return `0` immediately if nothing is waiting |
| `> 0` | Wait up to that long |

| Return | |
|---|---|
| `1` | An event was stored in `*out` |
| `0` | Timed out, or a message was consumed that produced no event |
| `-1` | Error, `errno` set — including a server that went away |

Two details are worth knowing before building a loop on it.

**An unknown message type is skipped, not an error.** Every message is framed with its own
length, so a message the client does not recognise is stepped over and the loop continues.
That is what lets a newer server talk to an older client without a version bump for every
added event — and it is why a `0` return does not strictly mean "timed out".

**The timeout is not a deadline.** After skipping an unrecognised message, a call with
`timeout_ms > 0` goes back to `poll()` with the full timeout again. A blocking call simply
reads again; a polling call (`0`) returns `0`. In practice the server sends nothing a
current client does not know, so this only matters across a version skew.

Reads are also not interruptible once a frame has started: a partially-received message is
read to completion. A peer that stops mid-frame blocks the call rather than returning short,
because there is no way to resynchronise a stream protocol from half a message.

`errno` after a `-1` is usually `ECONNRESET` (the server closed) or `EPROTO` (a framing or
length error). Both mean the connection is finished.

### The event

```c
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
```

| Type | | |
|---|---|---|
| `UI_EVENT_CONFIGURE` | `configure` | The window has a new size. Apply it before painting again. |
| `UI_EVENT_KEY` | `key` | A raw `KEY_*` code from `<aplus/input.h>`, down or up. Focused window only. |
| `UI_EVENT_POINTER` | `pointer` | Position relative to the content area, plus the button mask. |
| `UI_EVENT_LEAVE` | — | The pointer has left the content area. |
| `UI_EVENT_FOCUS` | `focus` | Keyboard focus gained or lost. |
| `UI_EVENT_CLOSE` | — | A close was requested. The application decides what that means. |

```c
#define UI_BUTTON_LEFT   (1 << 0)
#define UI_BUTTON_RIGHT  (1 << 1)
#define UI_BUTTON_MIDDLE (1 << 2)
```

`window_id` is set on every event. A client with one window can ignore it; a client with
several routes on it.

### What the server sends, and when

- **Keys** go to the focused window only. There is no keymap anywhere in the system: the
  server forwards raw `KEY_*` codes and leaves the question of what they mean to whoever
  needs characters. Modifiers arrive as ordinary key events, and a held key repeats as a
  stream of downs with no intervening up.
- **Pointer events** go to the window under the pointer, whether or not it has focus, and
  only while the pointer is over the **content** area — not the titlebar, borders or resize
  grips, which the server handles itself.
- **`UI_EVENT_LEAVE`** is sent when the pointer stops being over a window's content area.
  There is no matching "enter": arriving somewhere is already described by the
  `UI_EVENT_POINTER` that follows the pointer in. Leaving is the one transition that produces
  no event of its own, and without it a client that highlights whatever is under the pointer
  keeps the last thing it highlighted lit forever.
- **`UI_EVENT_FOCUS`** on gain says nothing about where the pointer is. If you track modifier
  state, clear it on focus *loss* — the release that would have cleared it goes to whoever has
  focus now.
- **`UI_EVENT_CLOSE`** comes from the titlebar button or `Ctrl+Alt+Q`. Nothing exits on your
  behalf.

## Multiple windows

One connection can carry as many windows as you like; `ui_window_create()` adds each to the
connection's list and `ui_next_event()` delivers events for all of them, tagged with
`window_id`. Look the window up yourself from the id and dispatch accordingly.

The widget layer does not do this for you: `ui_view_dispatch()` ignores `window_id` and
assumes the event is for its own window, so a multi-window application either keeps one view
per window and routes events itself, or uses the surface layer directly.

## Threading

The library is not thread-safe, and the shape of it assumes a specific split, which is why
`ui_window_apply_configure()` is separate from `ui_next_event()`: an event thread can read
events while a drawing thread owns the pixels, as long as the resize happens on the drawing
side.

Everything else is unsynchronised. Two threads committing on one connection interleave their
writes and corrupt the stream — commits are several `write()` calls each, and nothing holds a
lock across them. If you split event handling from drawing, keep all sending on one side.

## Framing helpers

```c
ssize_t ui_send_all(int fd, const void* buf, size_t size);
ssize_t ui_recv_all(int fd, void* buf, size_t size);
int     ui_send_msg(int fd, uint16_t type, const void* payload, size_t size);
```

Exported so that the server can share them and both ends agree byte for byte. An application
has no reason to call them unless it is extending the protocol; see
[protocol.md](protocol.md).

They exist because a local socket here is an ordinary descriptor: `write()` can return having
moved fewer bytes than asked, since `ringbuffer_write()` stops at the end of the peer's
buffer and reports what it managed. Every send has to loop, or a frame is silently truncated
and the stream desynchronises for good.
