# libui — the aplus UI client library

`libui` is what an aplus application links against to put a window on the screen. It is
the client half of the display system: the server half is `aplus-wm`, which owns the
framebuffer and the input devices and hands out windows over a Unix socket.

The library is deliberately two libraries stacked on top of each other. The lower one
deals in a rectangle of pixels and a stream of events; the upper one turns that into a
list of things that draw themselves and say when they have changed. An application picks
the layer that fits and never has to see the other.

```
   aplus-explorer   aplus-calculator   aplus-terminal         ui-test
          │                 │                 │                  │
          │       <aplus/ui-widgets.h>        │                  │  <aplus/ui.h>
          └─────────────────┴─────────────────┴─ libui (-lui) ───┘
                                   │
          control:  AF_UNIX /tmp/aplus-wm.sock
          pixels:   System V shared memory, one segment per window
                                   │
                                aplus-wm
                                   │
              /dev/fb0   /dev/kbd   /dev/mouse   /dev/tablet
```

A window's pixels never go down the socket. The server creates a shared memory segment per
window and both ends map it, so a client draws into the very memory the compositor reads and a
commit is a damage rectangle rather than a frame.

## The two layers

| | header | what it gives you | what it costs |
|---|---|---|---|
| **Surface layer** | `<aplus/ui.h>` | A connection, a window, a shared `uint32_t*` you write pixels into, damage tracking, commits, and an event queue. | You draw everything, including text. |
| **Widget layer** | `<aplus/ui-widgets.h>` | A view over that surface, a themed panel/label/button/list set, a grid, hit testing, keyboard focus, damage tracking per widget, and an event loop. | Cairo and FreeType get linked in. |

The widget layer is built on the public surface API and nothing else, so the two mix
freely: a view can be driven from your own event loop, and the raw pixels stay reachable
through `ui_window_pixels()` for the parts a widget cannot express.

Neither layer draws window decorations. The titlebar, borders, shadow, close button and
resize grips all belong to `aplus-wm`, and the coordinates a client sees — the size it is
configured with, the pointer positions it receives — are relative to the content area
inside them. A window created with `UI_WINDOW_BORDERLESS` gets none of them and is handed
its whole frame instead — see
[Borderless windows](window-api.md#borderless-windows).

## Where the code lives

| Path | |
|---|---|
| `lib/aplus/ui/include/aplus/ui.h` | Surface API and the wire protocol, shared with the server |
| `lib/aplus/ui/include/aplus/ui-widgets.h` | Widget API |
| `lib/aplus/ui/ui_connection.c` | Connect, handshake, framing helpers |
| `lib/aplus/ui/ui_window.c` | Window creation, the shared surface, resize, damage, commits |
| `lib/aplus/ui/ui_event.c` | Event decoding and `ui_next_event()` |
| `lib/aplus/ui/ui_view.c` | The view: surface binding, dispatch, paint, run loop |
| `lib/aplus/ui/ui_widget.c` | Widget base, grid, rect helpers |
| `lib/aplus/ui/ui_panel.c`, `ui_label.c`, `ui_button.c`, `ui_list.c` | The widgets themselves |
| `lib/aplus/ui/ui_theme.c` | The colour scheme and the default dark theme |
| `lib/aplus/ui/ui_draw.c`, `ui_font.c` | Cairo drawing helpers and the FreeType face cache |
| `lib/aplus/ui/ui_damage.c` | The damage set both the window and the view keep |
| `apps/sysutils/aplus-wm/` | The server |
| `apps/sysutils/aplus-calculator/` | The reference widget-layer application: a grid of buttons and a keyboard |
| `apps/sysutils/aplus-explorer/` | The other one: a list, a selection and a keyboard focus |
| `apps/sysutils/aplus-image-viewer/` | The one that mixes the layers: a toolbar of widgets around a picture drawn straight into the window pixels |
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

A graphical application also ships a desktop entry, which is what lets something else start it
by name rather than by path. `RESOURCES` is a prerequisite of the binary, so naming the
installed file there is enough to have it built:

```make
RESOURCES += $(SYSROOT)/usr/share/applications/ui-hello.desktop

$(SYSROOT)/usr/share/applications/ui-hello.desktop: assets/ui-hello.desktop
	$(QUIET)install -d $(@D)
	$(QUIET)install -m 644 $< $@
```

```ini
[Desktop Entry]
Name=Hello
Comment=Hello application
Exec=ui-hello
Terminal=false
Type=Application
Categories=Utility;
```

`aplus-xopen` is what reads them: given a path it picks a handler — a directory opens in
`aplus-explorer`, a picture in `aplus-image-viewer`, a `.desktop` file runs the `Exec` it names,
through `aplus-terminal` when it asks for one — and `execvp()`s it, so the caller ends up with
the application as its own child rather than with an opener in between.

## Running it

The display server has to be running before any client. `apps/core/init/scripts/init.sh`
starts it at boot:

```sh
aplus-wm &
aplus-terminal -c "cat /etc/motd && while true; do /bin/dash; done"
```

There is no sleep between the two lines because `ui_connect()` retries — see
[getting-started.md](getting-started.md#3-connecting).

The desktop behind the windows is a webp picture out of `/usr/share/images`, shipped by the
`system-images` package. The server decodes it once at startup and scales it to cover the screen,
cropping whichever axis is left over, so compositing the desktop stays a plain blit. When the file
is missing or does not decode, it falls back to the grey gradient and says so on the console.
`-w`/`--wallpaper` names a different one:

```sh
aplus-wm -w /usr/share/images/04.webp &
```

From a running terminal, a GUI application is started like any other program. A few server
bindings are worth knowing:

| Keys | |
|---|---|
| `Ctrl+Alt+T` | Spawn a new terminal |
| `Ctrl+Alt+Q` | Close the focused window (sends it `UI_EVENT_CLOSE`) |
| `Alt+Tab` | Focus and raise the next window down the stack |
| `Alt+Shift+Tab` | The same walk, the other way round |
| `Super`+drag | Move a window from anywhere on it, decorations or not |

Holding `Alt` down keeps the walk going: the order is taken when the first `Tab` arrives and
does not move again until `Alt` is released, so a second `Tab` reaches the third window rather
than returning to the first. Tapping the chord and letting go therefore swaps between the two
most recently used windows.

For a headless run with the console log captured, `./makew run-headless`.

`ui-test` is the smoke test for the pair of them: it opens a window, paints a gradient and
prints the keys, pointer, focus and configure events it receives, so a window that appears and
reports keys means the protocol works. It says nothing about leave or scroll events; a wheel
is easier to watch from a list in `aplus-explorer`.
`ui-test --once` paints one frame and exits, which is what makes the surface lifecycle
drivable from a script — every window costs a shared memory segment that both ends have to let
go of, and `SHM_SEGMENT_MAX` is 64, so a loop of a hundred that still creates its hundredth
window is a lifecycle with no leak in it.

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
- [Window and event API](window-api.md) — the surface layer in full: the shared pixels and
  their stride, damage, commits, configure serials, and the event queue.
- [Wire protocol](protocol.md) — the bytes on the socket and the shared memory behind them,
  for anyone writing a second client library or a second server.

## What is not here

The widget set is four widgets: a panel, a label, a button and a list. There is no text entry,
no checkbox, no menu, no nested container, and no layout engine beyond the grid helper — a
layout callback positions everything in absolute coordinates. Labels are one line, clipped,
and never wrap. There is no standalone scrollbar either: the list has one, but it belongs to
the list and cannot be put on anything else.

Keyboard focus exists but does not move by itself. A click focuses the widget it lands on, and
`ui_view_focus()` moves it deliberately; nothing walks the widgets on Tab, and the view has no
idea what order they would be walked in.

These are gaps rather than decisions, and the shape of the library is meant to absorb them: a
new widget is a `ui_widget_kind_t`, a table of hooks and a few setters.
