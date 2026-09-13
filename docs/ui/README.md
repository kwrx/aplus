# libui — the aplus UI client library

`libui` is what an aplus application links against to put a window on the screen. It is
the client half of the display system: the server half is `aplus-wm`, which owns the
framebuffer and the input devices and hands out windows over a Unix socket.

The library is deliberately two libraries stacked on top of each other. The lower one
deals in a rectangle of pixels and a stream of events; the upper one turns that into a
list of things that draw themselves and say when they have changed. An application picks
the layer that fits and never has to see the other.

```
   aplus-calculator          aplus-terminal              ui-test
          │                        │                        │
          │   <aplus/ui-widgets.h> │                        │  <aplus/ui.h>
          └────────────────── libui (-lui) ─────────────────┘
                                   │
                    AF_UNIX  /tmp/aplus-wm.sock
                                   │
                                aplus-wm
                                   │
              /dev/fb0   /dev/kbd   /dev/mouse   /dev/tablet
```

## The two layers

| | header | what it gives you | what it costs |
|---|---|---|---|
| **Surface layer** | `<aplus/ui.h>` | A connection, a window, a `uint32_t*` you write pixels into, damage tracking, commits, and an event queue. | You draw everything, including text. |
| **Widget layer** | `<aplus/ui-widgets.h>` | A view over that surface, a themed panel/label/button set, a grid, hit testing, damage tracking per widget, and an event loop. | Cairo and FreeType get linked in. |

The widget layer is built on the public surface API and nothing else, so the two mix
freely: a view can be driven from your own event loop, and the raw pixels stay reachable
through `ui_window_pixels()` for the parts a widget cannot express.

Neither layer draws window decorations. The titlebar, borders, shadow, close button and
resize grips all belong to `aplus-wm`, and the coordinates a client sees — the size it is
configured with, the pointer positions it receives — are relative to the content area
inside them.

## Where the code lives

| Path | |
|---|---|
| `lib/aplus/ui/include/aplus/ui.h` | Surface API and the wire protocol, shared with the server |
| `lib/aplus/ui/include/aplus/ui-widgets.h` | Widget API |
| `lib/aplus/ui/ui_connection.c` | Connect, handshake, framing helpers |
| `lib/aplus/ui/ui_window.c` | Window creation, resize, damage, commit banding |
| `lib/aplus/ui/ui_event.c` | Event decoding and `ui_next_event()` |
| `lib/aplus/ui/ui_view.c` | The view: surface binding, dispatch, paint, run loop |
| `lib/aplus/ui/ui_widget.c` | Widget base, grid, rect helpers |
| `lib/aplus/ui/ui_panel.c`, `ui_label.c`, `ui_button.c` | The widgets themselves |
| `lib/aplus/ui/ui_theme.c` | The colour scheme and the default dark theme |
| `lib/aplus/ui/ui_draw.c`, `ui_font.c` | Cairo drawing helpers and the FreeType face cache |
| `apps/sysutils/aplus-wm/` | The server |
| `apps/sysutils/aplus-calculator/` | The reference widget-layer application |
| `apps/test/ui-test/` | The reference surface-layer application |

## Building against it

`libui` is built and installed by the ordinary `./makew all`; the headers land in
`$(SYSROOT)/usr/include/aplus/` and the archive in `$(SYSROOT)/usr/lib/libui.a`.

An application that only uses the surface layer needs nothing beyond `-lui`. Because
`libui.a` is a static archive, the linker only pulls in the objects that are actually
referenced, so a client that never touches a view never drags cairo in:

```make
INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include
LIBS     += ui
include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
```

An application that uses the widget layer has to name the drawing stack as well:

```make
INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include

# The sysroot headers arrive through -isystem rather than INCLUDES (which becomes -I):
# musl's endian.h trips -Wparentheses, and with -Werror on that turns a system header
# into a build failure as soon as its directory is named as a user include path.
# -include config.h has to be repeated here because build-binary.mk only supplies it
# with ?=, which any CFLAGS assignment above it would silence.
CFLAGS   += -include $(ROOTDIR)/config.h
CFLAGS   += -isystem $(SYSROOT)/usr/include
CFLAGS   += -isystem $(SYSROOT)/usr/include/freetype2

LIBS     += ui cairo pixman-1 freetype png z

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
```

The `CFLAGS += -include $(ROOTDIR)/config.h` line is not optional decoration. `build-binary.mk`
supplies the config header with `?=`, so *any* `CFLAGS` assignment placed above the include
silently cancels it, every `CONFIG_*` macro becomes undefined, and code guarded by one
compiles down to its `#else` branch without a warning.

`LIBS` order matters: `ui` before `cairo`, `cairo` before its own dependencies. The
archives are searched left to right.

There is nothing else to register. `apps/Makefile` finds every directory under
`apps/{core,sysutils,extra,test}` that contains a `Makefile`, so a new application is a new
directory and nothing more.

## Running it

The display server has to be running before any client. `apps/core/init/scripts/init.sh`
starts it at boot:

```sh
aplus-wm &
aplus-terminal -c "cat /etc/motd && while true; do /bin/dash; done"
```

There is no sleep between the two lines because `ui_connect()` retries — see
[getting-started.md](getting-started.md#3-connecting).

From a running terminal, a GUI application is started like any other program. Two server
bindings are worth knowing:

| Keys | |
|---|---|
| `Ctrl+Alt+T` | Spawn a new terminal |
| `Ctrl+Alt+Q` | Close the focused window (sends it `UI_EVENT_CLOSE`) |

For a headless run with the console log captured, `./makew run-headless`.

## Documentation map

**Tutorials**

- [Getting started](getting-started.md) — a window, a panel, a label and a button, from an
  empty directory to a running application.
- [Layout and resizing](tutorial-layout.md) — the layout callback, the grid, and binding
  the keyboard to the same widgets the pointer drives.
- [Custom drawing](tutorial-custom-drawing.md) — dropping to raw pixels, and mixing your
  own cairo into a widget view.

**Reference**

- [Widgets](widgets.md) — the view, the widget set, hit testing, damage and the frame
  lifecycle.
- [Theming](theming.md) — the colour roles, the default dark scheme, and writing your own.
- [Window and event API](window-api.md) — the surface layer in full: pixels, damage,
  commits, configure serials, and the event queue.
- [Wire protocol](protocol.md) — the bytes on the socket, for anyone writing a second
  client library or a second server.

## What is not here

The widget set is three widgets: a panel, a label and a button. There is no text entry, no
checkbox, no scrollbar, no menu, no nested container, and no layout engine beyond the grid
helper — a layout callback positions everything in absolute coordinates. Labels are one
line, clipped, and never wrap. A view has no notion of keyboard focus between widgets: keys
arrive at one callback for the whole window and it decides what they mean.

These are gaps rather than decisions, and the shape of the library is meant to absorb them:
a new widget is a `ui_widget_kind_t`, a draw function and a few setters.
