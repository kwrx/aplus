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
| `0x0006` | `UI_REQ_RESIZE_WINDOW` | `ui_msg_resize_t` |
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
#define UI_PROTOCOL_VERSION 6
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

Version 4 added the `flags` field to `ui_msg_create_window_t`, and with it
`UI_WINDOW_BORDERLESS`. That one *is* a break: the payload grew, so a version 3 client's
create-window request is four bytes short of what a version 4 server reads and is refused on
its length. The handshake catches it first and says so.

Version 5 added `UI_WINDOW_CENTERED`, a bit in the field version 4 already carried. Not a
byte moved, and yet it is a break too, for the opposite reason to version 2's: because
unknown flag bits are *refused* rather than ignored, a version 5 client asking to be centred
is refused outright by a version 4 server. A flag that is validated cannot be added
compatibly, which is the price of finding out at creation instead of never.

Version 6 added `UI_WINDOW_TRANSLUCENT`, and is a break for the same reason version 5 was:
another validated bit in the same field. It is a flag rather than a property because the
format of the surface has to be settled before the segment is created, and that is before a
client has a window to say anything about — a window that changed format afterwards would
mean rewrapping a segment already being drawn into, for a thing no client wants to change
twice.

`UI_REQ_RESIZE_WINDOW` arrived in the same version and **would not have needed a bump of its
own**, which is why it is written the way it is. It is a new request, and an unknown request
is drained by its own length rather than being fatal — the escape hatch
[Extending it](#extending-it) names, which only holds where being ignored leaves the client
with something it can still use. A server that drops it leaves the window the size it was
created at, which is what a client that never sent it gets anyway, so nothing waits for an
answer that is not coming.

Nothing else moved: no payload grew, `ui_msg_configure_t` is untouched, and ARGB32 and RGB24
have the same stride and the same segment size, so the surface a translucent window is handed
is the same shape as any other. One flag bit is the whole of what version 6 breaks.

Unlike every other read in the client, the hello reply is read without skipping unknown
messages: nothing else can legitimately arrive before it.

## Creating a window

```
client → UI_REQ_CREATE_WINDOW  { width, height, flags, title[64] }
server → UI_EV_CONFIGURE       { window_id, serial, width, height, shm_id, stride, shm_size }
```

The window id is assigned by the server and first appears in that configure, together with the
segment the window is to be drawn into. The size is likewise the server's: the request is a
hint, clamped to at least 80×40 and to what fits on screen.

`flags` is `UI_WINDOW_*`, and it is the one field of this protocol that is validated rather
than ignored:

```c
#define UI_WINDOW_DECORATED   0
#define UI_WINDOW_BORDERLESS  (1 << 0)
#define UI_WINDOW_CENTERED    (1 << 1)
#define UI_WINDOW_TRANSLUCENT (1 << 2)
#define UI_WINDOW_FLAGS_ALL   (UI_WINDOW_BORDERLESS | UI_WINDOW_CENTERED | UI_WINDOW_TRANSLUCENT)
```

A bit the server does not know closes the connection, on the reasoning that a client asking
for a kind of window that does not exist would otherwise be handed an ordinary one and never
find out. `UI_WINDOW_BORDERLESS` asks the server to draw nothing around the window: no
titlebar, no border, no close button, and the content area covering the whole
frame — which in turn means pointer events over all of it, no edge to resize from, and Super
held down as the only way to drag it. What it does keep is the corners: every window the
server draws is rounded off by `UI_WINDOW_RADIUS`, decorated or not, and a borderless one is
clipped to that curve with its shadow cast around the same one. It can still ask for a size
with [`UI_REQ_RESIZE_WINDOW`](#resizing). `UI_WINDOW_CENTERED` asks for the frame to be placed
in the middle of the display rather than on the cascade. `UI_WINDOW_TRANSLUCENT` asks for
[a surface that carries alpha](#the-window-surface).

Placement is a creation flag because it is the only point at which a client has any say in
it: there is no move request and no message carrying the screen size, so a window either
names where it wants to be here or takes what the server gives it. Flags are fixed for the
life of the window; there is no request to change them, and the resize request above changes
a property rather than a flag. `UI_WINDOW_TRANSLUCENT` is a flag rather than a property for a
different reason from the placement one: the server picks the surface's cairo format when it
creates the segment, and that happens before the client has been told the window exists.

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

The format is `0xFFRRGGBB` (`CAIRO_FORMAT_RGB24`), not premultiplied. The alpha byte is
ignored, and the window is drawn over whatever is behind it.

A `UI_WINDOW_TRANSLUCENT` window is the exception: its surface is `CAIRO_FORMAT_ARGB32`
instead, premultiplied, and the server blends it over the desktop and the windows below. The
two formats have the same stride and the same segment size — cairo picks four bytes a pixel
either way — so nothing about the configure, the bounds check or the segment changes, only
what the fourth byte of a pixel means. Which of the two a window gets is settled when it is
created and does not change.

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

## Resizing

```c
typedef struct {

    uint32_t window_id;
    uint16_t width;
    uint16_t height;

} __attribute__((packed)) ui_msg_resize_t;
```

The client end of the resize a drag on a window edge starts. It is a request in the weak
sense: the server clamps it the way it clamps a creation, and answers with a
`UI_EV_CONFIGURE` naming the size it settled on — or with nothing at all, when that is the
size the window already had. A client that treats the request as having worked will paint at
a size it was never given.

The window keeps its origin. `UI_WINDOW_CENTERED` places a window when it is created and
nothing re-derives it afterwards, so a window that grows grows downward and to the right
rather than out from the middle. For the launcher, which is the reason this exists, that is
also what is wanted: its search field stays where it was put while the results below it come
and go.

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
titlebar, borders and resize grips are the server's. A `UI_WINDOW_BORDERLESS` window has none
of those, so its content area and its frame are the same rectangle.

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

`UI_EV_CONFIGURE` is sent on creation, on a `UI_REQ_RESIZE_WINDOW` that changed something,
and once at the end of a resize drag rather than on every mouse packet — a client repainting at a hundred sizes a second is the thing being
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
