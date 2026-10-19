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
window and both ends map it. A client draws into a back buffer of its own, and a commit copies
the damaged rectangles into the segment the compositor reads, then sends those rectangles
rather than a frame.

## The two layers

| | header | what it gives you | what it costs |
|---|---|---|---|
| **Surface layer** | `<aplus/ui.h>` | A connection, a window, a double-buffered `uint32_t*` you write pixels into, damage tracking, commits, and an event queue. | You draw everything, including text. |
| **Widget layer** | `<aplus/ui-widgets.h>` | A view over that surface, a themed panel/label/button/list set, a grid, hit testing, keyboard focus, damage tracking per widget, and an event loop. | Cairo and FreeType get linked in. |

The widget layer is built on the public surface API and nothing else, so the two mix
freely: a view can be driven from your own event loop, and the raw pixels stay reachable
through `ui_window_pixels()` for the parts a widget cannot express.

Neither layer draws window decorations. The titlebar and its icon, borders, shadow, close, maximise
and minimise buttons, resize grips and the taskbar all belong to `aplus-wm`, and the coordinates a client sees — the size it is
configured with, the pointer positions it receives — are relative to the content area
inside them. A window created with `UI_WINDOW_BORDERLESS` gets none of them and is handed
its whole frame instead — see
[Window flags](window-api.md#window-flags).

## Where the code lives

| Path | |
|---|---|
| `lib/aplus/ui/include/aplus/ui.h` | Surface API and the wire protocol, shared with the server |
| `lib/aplus/ui/include/aplus/ui-widgets.h` | Widget API |
| `lib/aplus/ui/include/aplus/ui-anim.h`, `ui_anim.c` | Easing and transitions, shared by the widgets and the server — see [Animation](animation.md) |
| `lib/aplus/ui/ui_connection.c` | Connect, handshake, framing helpers |
| `lib/aplus/ui/ui_window.c` | Window creation, the shared surface and its back buffer, resize, damage, commits |
| `lib/aplus/ui/ui_event.c` | Event decoding and `ui_next_event()` |
| `lib/aplus/ui/ui_view.c` | The view: surface binding, dispatch, paint, run loop |
| `lib/aplus/ui/ui_widget.c` | Widget base, grid, rect helpers |
| `lib/aplus/ui/ui_panel.c`, `ui_label.c`, `ui_button.c`, `ui_list.c`, `ui_entry.c` | The widgets themselves |
| `lib/aplus/ui/ui_keymap.c` | The console keymap, for the widget that needs characters rather than key codes |
| `lib/aplus/ui/ui_theme.c` | The colour scheme and the default dark theme |
| `lib/aplus/ui/ui_draw.c`, `ui_font.c` | Cairo drawing helpers and the FreeType face cache |
| `lib/aplus/ui/ui_icon.c` | Icons by name, out of the theme in `/usr/share/icons` |
| `lib/aplus/ui/assets/icons/` | The default icon theme, and the SVG sources it is rendered from |
| `lib/aplus/ui/ui_damage.c` | The damage set both the window and the view keep |
| `apps/sysutils/aplus-wm/` | The server |
| `apps/sysutils/aplus-calculator/` | The reference widget-layer application: a grid of buttons and a keyboard |
| `apps/sysutils/aplus-explorer/` | The other one: a list, a selection and a keyboard focus |
| `apps/sysutils/aplus-viewer/` | The one that mixes the layers: a toolbar of widgets around a PDF or a picture that mupdf renders a tile at a time straight into the window pixels, and the canvas scrolls, zooms and drags over |
| `apps/sysutils/aplus-editor/` | The bigger mix: a toolbar, a prompt bar and a status line of widgets around a text area it draws, damages and blinks a caret in by itself |
| `apps/sysutils/aplus-launcher/` | The one with a search field: an entry driving a list, in a translucent window only as tall as it has results — borderless and centred, or a drawer out of the taskbar's rail with `--drawer` |
| `apps/test/ui-test/` | The reference surface-layer application |

## Building against it

`libui` is built and installed by the ordinary `./makew all`; the headers land in
`$(SYSROOT)/usr/include/aplus/` and the archive in `$(SYSROOT)/usr/lib/libui.a`.

An application that only uses the surface layer needs nothing beyond `-lui` and the `-lz`
the keymap is read through. Because `libui.a` is a static archive, the linker only pulls in
the objects that are actually referenced, so a client that never touches a view never drags
cairo in:

```make
INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include
LDFLAGS  += -L$(ROOTDIR)/lib/aplus/ui
LIBS     += ui z
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

LDFLAGS  += -L$(ROOTDIR)/lib/aplus/ui

LIBS     += ui cairo pixman-1 freetype png z

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
```

The `CFLAGS += -include $(ROOTDIR)/config.h` line is not optional decoration. `build-binary.mk`
supplies the config header with `?=`, so *any* `CFLAGS` assignment placed above the include
silently cancels it, every `CONFIG_*` macro becomes undefined, and code guarded by one
compiles down to its `#else` branch without a warning.

The `LDFLAGS += -L$(ROOTDIR)/lib/aplus/ui` line is not optional either, and what it prevents
is worse than a build error. `build-binary.mk` links against `$(SYSROOT)/usr/lib`, which holds
the copy of `libui.a` installed by the *last* `./makew install` — so without this line, editing
the library and rebuilding an application links it against the old archive and the change
simply does not appear. `LDFLAGS` is placed ahead of the sysroot directory on the link line, so
naming the build directory here picks up the archive that was just built.

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
Icon=ui-hello
Terminal=false
Type=Application
Categories=Utility;
```

`Icon` names an icon in the theme rather than a file — see [Icons](icons.md) — and an
application whose icon the theme has never heard of is listed with a generic one rather than
with none.

`aplus-xopen` is what reads them: given a path it picks a handler — a directory opens in
`aplus-explorer`, a `.pdf` or a picture in `aplus-viewer`, a text file in `aplus-editor` (and so
does a file with no known extension and no NUL byte in its first 4 KiB), a `.nes` ROM in `nesemu`,
a `.desktop` file runs the `Exec` it names, through `aplus-terminal` when it asks for one — and
`execvp()`s it, so the caller ends up with the application as its own child rather than with an
opener in between.

`aplus-launcher` is what lists them. It reads the other half of the same files — the `Name`,
`Comment` and `Icon` a person picks an application by — and hands the path to `aplus-xopen` rather than
running anything itself, which is also how it stays out of the way of the process tree: it
`execvp()`s the opener in place, so the application inherits the launcher's parent.

## Running it

The display server has to be running before any client. `apps/core/init/scripts/init.sh`
starts it at boot, and nothing else:

```sh
aplus-wm &
sleep infinity
```

The desktop comes up empty; `Ctrl+Alt+T` opens a terminal. A client started from the same script
needs no sleep before it, because `ui_connect()` retries — see
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
| `Super+Up` | Maximise the focused window, or put it back |
| `Super+Down` | Minimise the focused window into the taskbar |
| `Super`+drag | Move a window from anywhere on it, decorations or not |

A double click on a titlebar maximises the window too, and so does the button beside the close
button. Dragging a maximised window by its titlebar puts it back at its old size under the
pointer. Windows open centred, stepping down and right from one already there, and they grow
into place, fade away when they close, and morph between their two sizes when maximised —
[Animation](animation.md#what-the-server-animates) lists every transition the server runs.

The taskbar is a rail down the left edge of the display, joined to a thin border round the
rest of it, and windows are placed, clamped and maximised within the rounded area the two
leave. At the top of the rail is the launcher's button, a grid of dots whose middle cross is
in the accent. It starts the launcher as `Super+Space` does, but with `--all --drawer`, so it
lists every application rather than an empty field and slides out of the rail as a drawer in
the top-left corner instead of opening in the middle. While the drawer is out the button's
cross turns into a close cross; a second click, `Escape`, or a press anywhere off the drawer
and the taskbar puts it away.

Below the button is an entry for every decorated window, in the order the windows opened,
with its icon; the active one is lit and marked in the accent against the rail's edge, and its
title runs up the free stretch of rail under the entries. The foot of the rail shows the time,
the hours over the minutes. Hovering an item pops a label out of the rail beside it: the
window's title, `Applications` for the launcher's button, the date for the clock. Clicking an
entry brings that window to the front, or hides it if it is already there, and brings a hidden
one back. The third titlebar button hides a window too, which then shrinks into its entry. A
borderless window gets no entry, and `Alt+Tab` steps over the hidden ones.

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
- [Animation](tutorial-animation.md) — a value that glides and a pulse that knows when to stop,
  stepped by a loop of your own alongside the widgets' transitions.

**Reference**

- [Widgets](widgets.md) — the view, the widget set, hit testing, damage and the frame
  lifecycle.
- [Animation](animation.md) — transitions, the curves and the shared durations, how frames
  are scheduled, and what the widgets and the server animate by themselves.
- [Theming](theming.md) — the colour roles, the default dark scheme, and writing your own.
- [Icons](icons.md) — the theme in `/usr/share/icons`, how a name is resolved, and what
  `Icon` in a desktop entry buys.
- [Window and event API](window-api.md) — the surface layer in full: the shared pixels and
  their stride, damage, commits, configure serials, and the event queue.
- [Wire protocol](protocol.md) — the bytes on the socket and the shared memory behind them,
  for anyone writing a second client library or a second server.

## What is not here

The widget set is five widgets: a panel, a label, a button, a list and a single-line entry.
There is no checkbox, no menu, no nested container, and no layout engine beyond the grid
helper — a layout callback positions everything in absolute coordinates. Labels are one line,
clipped, and never wrap; so is the entry, and it has no selection to cut or paste. There is no
standalone scrollbar either: the list has one, but it belongs to the list and cannot be put on
anything else.

Keyboard focus exists but does not move by itself. A click focuses the widget it lands on, and
`ui_view_focus()` moves it deliberately; nothing walks the widgets on Tab, and the view has no
idea what order they would be walked in.

These are gaps rather than decisions, and the shape of the library is meant to absorb them: a
new widget is a `ui_widget_kind_t`, a table of hooks and a few setters.
