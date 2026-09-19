# Wire protocol

The bytes between a client and `aplus-wm`. `libui` implements this, and an application
using `libui` never needs to read this page — it is here for anyone writing a second client
library, a second server, or a new message type.

The definitions live in `lib/aplus/ui/include/aplus/ui.h`, which both ends include. There is
no separate server header: the protocol is the shared header, which is what keeps the two
implementations from drifting.

## Transport

`AF_UNIX`, `SOCK_STREAM`, at `/tmp/aplus-wm.sock` by default.

The socket carries control only. A window's pixels live in a System V shared memory segment
that both ends map, so the largest thing that ever travels over it is a window title — see
[the window surface](#the-window-surface).

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

A payload longer than this is refused by both ends. Nothing in this protocol is large any
more — the biggest message is a title — but a peer could still name any length it liked, and
the cap is what stops that from becoming an arbitrary allocation.

## Messages

Requests are client → server and have opcodes below `0x8000`; events are server → client and
have the high bit set.

| Opcode | | Payload |
|---|---|---|
| `0x0001` | `UI_REQ_HELLO` | `ui_msg_hello_t` |
| `0x0002` | `UI_REQ_CREATE_WINDOW` | `ui_msg_create_window_t` |
| `0x0003` | `UI_REQ_COMMIT` | `ui_msg_commit_t` |
| `0x0004` | `UI_REQ_SET_TITLE` | `ui_msg_set_title_t` |
| `0x0005` | `UI_REQ_DESTROY_WINDOW` | `ui_msg_window_t` |
| `0x8001` | `UI_EV_HELLO` | `ui_msg_hello_t` |
| `0x8002` | `UI_EV_CONFIGURE` | `ui_msg_configure_t` |
| `0x8003` | `UI_EV_KEY` | `ui_msg_key_t` |
| `0x8004` | `UI_EV_POINTER` | `ui_msg_pointer_t` |
| `0x8005` | `UI_EV_FOCUS` | `ui_msg_focus_t` |
| `0x8006` | `UI_EV_CLOSE` | `ui_msg_window_t` |
| `0x8007` | `UI_EV_LEAVE` | `ui_msg_window_t` |
| `0x8008` | `UI_EV_SCROLL` | `ui_msg_scroll_t` |

Every payload except the hello and the create-window begins with a `uint32_t window_id`.

There is no reply channel and no request ids. Requests are fire-and-forget; the only
synchronous exchanges are the handshake and window creation, and both work by waiting for a
specific event rather than by correlating a reply.

## Handshake

```
client → UI_REQ_HELLO   { version }
server → UI_EV_HELLO    { version }
```

```c
#define UI_PROTOCOL_VERSION 3
```

The client sends its version and compares what comes back; a mismatch is fatal on the client
side (`EPROTONOSUPPORT`). Nothing is negotiated — the exchange establishes agreement or ends
the connection.

Version 2 is where the pixels left the socket. In version 1 a commit carried its rectangle as
a trailing payload, cut into tiles small enough to fit the socket buffer, and the server
stitched them back together into its own copy of the surface. In version 2 the surface is
shared memory and a commit is the rectangle alone. The two are not compatible in either
direction, which is what the bump is for.

Version 3 added `UI_EV_SCROLL`. That one is not a break in either direction — an older client
skips an event it does not know, and a server that never sends one is a mouse without a wheel
— so the bump is bookkeeping rather than a barrier. Both ends are built and installed
together here, and a single number that names the whole protocol is easier to reason about
than a list of which features a peer happens to have.

Unlike every other read in the client, the hello reply is read without skipping unknown
messages: nothing else can legitimately arrive before it.

## Creating a window

```
client → UI_REQ_CREATE_WINDOW  { width, height, title[64] }
server → UI_EV_CONFIGURE       { window_id, serial, width, height, shm_id, stride, shm_size }
```

The window id is assigned by the server and first appears in that configure, together with the
segment the window is to be drawn into. The size is likewise the server's: the request is a
hint, clamped to at least 80×40 and to what fits on screen.

The client waits for the configure, stepping over anything else that turns up rather than
treating it as a protocol error — a client with no window yet has nothing to do with a stray
event, and this keeps the handshake from depending on the server's queue order.

Titles are a fixed 64-byte field (`UI_TITLE_MAX`), truncated, not necessarily
null-terminated by the sender's intent but always written from a zeroed struct.

## The window surface

A window's pixels are a System V shared memory segment. The server creates it, wraps its own
cairo backstore over it, and names it in the configure; the client attaches it with `shmat(2)`,
and `ui_window_pixels()` hands back a pointer into the very memory compositing reads. There is
no copy in either direction, and no pixel ever goes down the socket.

```c
typedef struct {

    uint32_t window_id;
    uint32_t serial;
    uint16_t width;
    uint16_t height;

    int32_t  shm_id;
    uint32_t stride;
    uint32_t shm_size;

} __attribute__((packed)) ui_msg_configure_t;
```

`stride` is bytes per row and is **not** `width * 4`. Cairo picks the stride for an image
surface (`cairo_format_stride_for_width()`), and since the two ends are writing and reading the
same memory they have to agree on it byte for byte. Index rows by it:

```c
uint32_t* row = (uint32_t*)((uint8_t*)pixels + (size_t)y * stride);
```

`shm_size` is the size of the segment, so that a client can check that what it attached is
large enough for the surface it was told about. `libui` refuses a configure whose `stride` is
below `width * 4`, or whose `shm_size` is below `stride * height`, with `EPROTO` — which is
what keeps a bad or truncated configure from becoming a write past the end of the segment.

The format is `0xFFRRGGBB` (`CAIRO_FORMAT_RGB24`), not premultiplied. Windows are opaque and
the alpha byte is ignored.

This is single buffered. A surface drawn into while it is being composited can tear; double
buffering would cost a second surface per window and a copy per frame, which is precisely what
the shared segment exists to avoid.

### Who owns the segment

The server, and that is what makes the lifecycle work out. It removes the segment (`shmctl(2)`
`IPC_RMID`) as soon as the window is destroyed or resized, and the kernel keeps a removed
segment alive until its last holder detaches. So:

- A client still drawing into the previous surface is never pulled out from under it. It does
  not learn about the new one until it acts on the configure, and its commits against the old
  one are dropped by the serial check in the meantime.
- A client that dies takes its attachment with it through address space teardown.
- The frames come back when both ends have let go, with no handshake to say when that is.

The client's whole share of this is the `shmdt(2)` that `libui` does in
`ui_window_apply_configure()` and `ui_window_destroy()`. Two ceilings are worth knowing before
writing that by hand, because both are low:

| | |
|---|---|
| `SHM_ATTACH_MAX` | 16 attachments per address space |
| `SHM_SEGMENT_MAX` | 64 segments system-wide |

The per-address-space one binds first and is the one a client can trip on its own. `libui`
attaches the new surface before detaching the old one, so a window costs one attachment at
rest and two across a resize; a client that attached on every configure without detaching
would stop being able to resize after sixteen. It bounds the server the same way — one
attachment per window it composites — so it is really a ceiling on windows open at once, not
on windows ever created. The system-wide one is what a server failing to `IPC_RMID` would
exhaust, at the 65th window.

Creating and destroying a hundred windows through the real server, with `ui-test --once`, is
the test that neither happens.

Nothing is remapped when the id has not changed. The server sends a configure at the end of
every drag, not only the ones that changed the size, and only a size change makes it build a
new segment; re-attaching the one already held would work, but costs a range of address space
that `shmdt(2)` does not give back. An id is never reused while the segment it names is alive,
so the same id is the same memory.

The server carries the old contents over into a new segment. Without that a window goes black
for the whole of a resize drag, since the client is not told the new size until the mouse is
released.

## Commits

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

Nothing follows this struct. The client has already written the pixels through the mapping
both ends share; the commit is a statement about which rectangle changed, and the server
damages that region of the screen. One fixed-size message however much was drawn — a
full-screen repaint costs what a single character cell costs. The server validates the payload
length against `sizeof()` and drops the message otherwise.

A frame can be more than one commit. `libui` keeps a handful of damage rectangles rather than
the box around them all, and sends one message per rectangle, each carrying the same serial —
see [Damage and commits](window-api.md#damage-and-commits). The server has always acted on
each commit on its own, so nothing about this is new on its side: a terminal that changed a
cell at the top and the cursor at the bottom now says so in two messages instead of claiming
everything between them.

### The serial

`serial` is echoed back from the last `UI_EV_CONFIGURE` the client drew against. The server
compares it and **silently drops a commit whose serial is stale**:

```c
    if (req.serial != win->serial) {
        return 0;
    }
```

A stale serial means the window was resized while the frame was in flight, so the rectangle
belongs to the surface the window used to have. Those writes went into the old segment, which
is still mapped for the client and which nothing on the server reads any more. Dropping the
commit is correct: a fresh `UI_EV_CONFIGURE` has already been queued with the new segment, and
the client will redraw into that.

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
    int16_t dx;
    int16_t dy;
} __attribute__((packed)) ui_msg_scroll_t;
```

A wheel step, in detents, going to the window under the pointer like any other pointer input.
Positive `dy` is away from the user, which scrolls a view towards its start; `dx` is the
horizontal wheel, which no input device in the tree produces today. It is a message of its own
rather than a field in `ui_msg_pointer_t` because a pointer event goes out on every motion,
and a client would have to tell a real detent from the zero carried by the other hundred.

The wheel reaches the server as `ev_rel.z` from `/dev/mouse`. Both the PS/2 and the
virtio-input drivers fill it, and the PS/2 one is normalised to the sign virtio-input already
used, so the convention above holds whatever the pointer is.

`UI_EV_CONFIGURE` is sent on creation, and once at the end of a resize drag rather than on
every mouse packet — a client repainting at a hundred sizes a second is the thing being
avoided. Its payload is [the window surface](#the-window-surface). The serial changes only
when the size actually did.

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
2. Send it with `ui_send_msg()`. Every message in the protocol is a fixed-size struct; if a
   new one needs a variable-length body, it has to be written by hand and read the same way.
3. Handle it: a case in `ui_next_event()` for an event, or in the server's request dispatch
   in `apps/sysutils/aplus-wm/src/client.c` for a request.
4. Validate the payload length against `sizeof()` before reading the body. Every existing
   case does this, and it is the only thing standing between a malformed frame and a read
   into the wrong bytes.

Do not reuse an opcode. An old peer that recognises the number but not the meaning is worse
than one that skips it.
