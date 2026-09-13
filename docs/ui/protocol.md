# Wire protocol

The bytes between a client and `aplus-wm`. `libui` implements this, and an application
using `libui` never needs to read this page — it is here for anyone writing a second client
library, a second server, or a new message type.

The definitions live in `lib/aplus/ui/include/aplus/ui.h`, which both ends include. There is
no separate server header: the protocol is the shared header, which is what keeps the two
implementations from drifting.

## Transport

`AF_UNIX`, `SOCK_STREAM`, at `/tmp/aplus-wm.sock` by default.

There is no `SOCK_SEQPACKET` here, so message boundaries do not survive the transport: every
message carries its own length and both sides reassemble frames by hand. A local socket is an
ordinary descriptor, so `write()` can return having moved fewer bytes than asked —
`ringbuffer_write()` stops at the end of the peer's buffer and reports what it managed. Every
send therefore loops (`ui_send_all()`), or a frame is silently truncated and the stream
desynchronises permanently.

A zero-length read means the peer closed. For a half-received frame that is an error, not a
short result: there is no way to resynchronise.

Structs are `__attribute__((packed))` and are written as raw memory, in host byte order.
Both ends are the same machine, so no conversion is done; the protocol is not portable
across an endianness boundary, and does not need to be.

## Framing

Every message is a header followed by exactly `length` bytes of payload:

```c
typedef struct {

    uint16_t type;
    uint16_t flags;
    uint32_t length;

} __attribute__((packed)) ui_msg_header_t;
```

`flags` is unused and sent as zero.

```c
#define UI_MSG_PAYLOAD_MAX (32 * 1024)
```

A payload longer than this is refused by both ends. A socket buffer is `CONFIG_PIPESIZ`
(65535) bytes and a write only ever makes partial progress, so a frame larger than the buffer
could never be written in one go; the cap also stops a malformed length from making the peer
allocate arbitrarily.

## Messages

Requests are client → server and have opcodes below `0x8000`; events are server → client and
have the high bit set.

| Opcode | | Payload |
|---|---|---|
| `0x0001` | `UI_REQ_HELLO` | `ui_msg_hello_t` |
| `0x0002` | `UI_REQ_CREATE_WINDOW` | `ui_msg_create_window_t` |
| `0x0003` | `UI_REQ_COMMIT` | `ui_msg_commit_t` + pixels |
| `0x0004` | `UI_REQ_SET_TITLE` | `ui_msg_set_title_t` |
| `0x0005` | `UI_REQ_DESTROY_WINDOW` | `ui_msg_window_t` |
| `0x8001` | `UI_EV_HELLO` | `ui_msg_hello_t` |
| `0x8002` | `UI_EV_CONFIGURE` | `ui_msg_configure_t` |
| `0x8003` | `UI_EV_KEY` | `ui_msg_key_t` |
| `0x8004` | `UI_EV_POINTER` | `ui_msg_pointer_t` |
| `0x8005` | `UI_EV_FOCUS` | `ui_msg_focus_t` |
| `0x8006` | `UI_EV_CLOSE` | `ui_msg_window_t` |
| `0x8007` | `UI_EV_LEAVE` | `ui_msg_window_t` |

Every payload except the hello and the commit begins with a `uint32_t window_id`.

There is no reply channel and no request ids. Requests are fire-and-forget; the only
synchronous exchanges are the handshake and window creation, and both work by waiting for a
specific event rather than by correlating a reply.

## Handshake

```
client → UI_REQ_HELLO   { version }
server → UI_EV_HELLO    { version }
```

```c
#define UI_PROTOCOL_VERSION 1
```

The client sends its version and compares what comes back; a mismatch is fatal on the client
side (`EPROTONOSUPPORT`). Nothing is negotiated — the exchange establishes agreement or ends
the connection.

Unlike every other read in the client, the hello reply is read without skipping unknown
messages: nothing else can legitimately arrive before it.

## Creating a window

```
client → UI_REQ_CREATE_WINDOW  { width, height, title[64] }
server → UI_EV_CONFIGURE       { window_id, serial, width, height }
```

The window id is assigned by the server and first appears in that configure. The size is
likewise the server's: the request is a hint, clamped to at least 80×40 and to what fits on
screen.

The client waits for the configure, stepping over anything else that turns up rather than
treating it as a protocol error — a client with no window yet has nothing to do with a stray
event, and this keeps the handshake from depending on the server's queue order.

Titles are a fixed 64-byte field (`UI_TITLE_MAX`), truncated, not necessarily
null-terminated by the sender's intent but always written from a zeroed struct.

## Commits and banding

```c
typedef struct {

    uint32_t window_id;
    uint32_t serial;

    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;

} __attribute__((packed)) ui_msg_commit_t;
```

The payload is this struct followed by `width * height * 4` bytes of `0xFFRRGGBB` pixels,
row by row, for the sub-rectangle named by `x`, `y`, `width`, `height`. The server validates
that the payload length matches exactly and drops the message otherwise.

```c
#define UI_COMMIT_BAND_MAX (16 * 1024)
```

A commit larger than this is split by the sender into tiles that each stay well inside the
socket buffer, so a commit always makes forward progress even when the peer is slow to
drain. Each tile names its own position, so the server reassembles them without knowing they
were ever one rectangle. Within a tile, rows are sent one `write()` each: the damage
rectangle is a sub-rectangle of a wider surface, so its rows are not contiguous in memory.

### The serial

`serial` is echoed back from the last `UI_EV_CONFIGURE` the client drew against. The server
compares it and **silently drops a commit whose serial is stale**:

```c
    /* The window was resized while this frame was in flight: it describes a surface that
       no longer exists. Dropping it is correct -- a fresh UI_EV_CONFIGURE has already been
       queued and the client will redraw at the new size. */
    if (req.serial != win->serial) {
        return 0;
    }
```

This is what makes a resize that races an in-flight frame harmless instead of an overrun: a
frame describing a surface that no longer exists is discarded, and the client has already
been told to redraw.

The consequence for a client is the rule in
[window-api.md](window-api.md#resizing): adopt the new serial by applying the configure, or
paint forever into a void.

## Events

```c
typedef struct {
    uint32_t window_id;
    uint16_t vkey;
    uint8_t down;
} __attribute__((packed)) ui_msg_key_t;
```

`vkey` is a raw `KEY_*` code from `<aplus/input.h>`, exactly as it came out of `/dev/kbd`.
The server owns no keymap: translation stays wherever the characters are actually needed.
Keys go to the focused window only.

```c
typedef struct {
    uint32_t window_id;
    int16_t x;
    int16_t y;
    uint8_t buttons;
} __attribute__((packed)) ui_msg_pointer_t;
```

Coordinates are relative to the content area, which excludes the server-drawn decorations,
and can legitimately be negative at the moment the pointer leaves. Events go to the window
under the pointer, regardless of focus, and only while it is over the content area — the
titlebar, borders and resize grips are the server's.

`UI_EV_LEAVE` follows when the pointer stops being over that content area. There is no
matching enter event: arriving is described by the `UI_EV_POINTER` that follows the pointer
in, and leaving is the one transition that would otherwise produce nothing.

```c
typedef struct {
    uint32_t window_id;
    uint32_t serial;
    uint16_t width;
    uint16_t height;
} __attribute__((packed)) ui_msg_configure_t;
```

Sent on creation and on every resize, each with a new serial.

## Extending it

Unknown message types are skipped, not fatal. Both ends read the header, and a type they do
not recognise is drained by its own length and the loop continues. That is what lets a newer
server talk to an older client without a version bump for every added event.

So a new **event** is backward compatible: old clients ignore it. A new **request** is not,
in the other direction — an old server ignoring a request the client depends on leaves the
client waiting for something that will never arrive. Adding one means either bumping
`UI_PROTOCOL_VERSION`, or designing the request so that being ignored is survivable.

To add a message:

1. Define the opcode and its packed payload struct in `lib/aplus/ui/include/aplus/ui.h`,
   alongside the others. Both ends pick it up from there.
2. Send it with `ui_send_msg()`, or by hand if it has a trailing variable-length body like a
   commit does.
3. Handle it: a case in `ui_next_event()` for an event, or in the server's request dispatch
   in `apps/sysutils/aplus-wm/src/client.c` for a request.
4. Validate the payload length against `sizeof()` before reading the body. Every existing
   case does this, and it is the only thing standing between a malformed frame and a read
   into the wrong bytes.

Do not reuse an opcode. An old peer that recognises the number but not the meaning is worse
than one that skips it.
