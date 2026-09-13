# Getting started

This builds a complete application: a window with a number in it and two buttons that
change the number. It is around 180 lines, it resizes properly, and it touches every part
of the widget layer — the connection, the window, the view, the widgets, the layout
callback and the run loop.

The finished article, if you would rather read that first, is
`apps/sysutils/aplus-calculator/main.c`: the same shape with twenty buttons instead of two.

## 1. The directory

An application is a directory with a `Makefile` and a `main.c`. Nothing registers it:
`apps/Makefile` walks `apps/{core,sysutils,extra,test}` and builds every directory that
contains a `Makefile`.

```bash
mkdir -p apps/extra/ui-hello
```

The binary is named after the directory, so this one installs as `/usr/bin/ui-hello`.

## 2. The Makefile

`apps/extra/ui-hello/Makefile`:

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

Copy the comment along with the lines it explains. Both traps it describes are silent in
different ways: the first turns a system header into a build error, the second compiles
every `CONFIG_*` block out of your program while the build still succeeds.

If your application never creates a view — no widgets, just pixels — `LIBS += ui` on its own
is enough and the three `CFLAGS` lines can go. `libui.a` is a static archive, so the linker
only pulls in the objects you actually reach, and cairo never enters the link.

## 3. Connecting

```c
ui_connection_t* conn = ui_connect(NULL, 5000);
```

`NULL` means `UI_DEFAULT_SOCKET`, which is `/tmp/aplus-wm.sock`. The socket lives on tmpfs
rather than on the root filesystem because binding one creates a real `S_IFSOCK` directory
entry, and ext2 here cannot store that.

The second argument is a retry window in milliseconds. The server is usually started from
the same script a moment earlier, so the socket may not exist yet; retrying for a few
seconds is what removes the need for a `sleep` between the two lines of `init.sh`. Pass `0`
to fail immediately.

`ui_connect()` performs the version handshake before it returns, so a connection you get
back is one that agrees on `UI_PROTOCOL_VERSION`.

## 4. The window

```c
ui_window_t* window = ui_window_create(conn, 260, 200, "ui-hello");
```

The size is the *content* area, inside the decorations the server draws. It is a request:
`ui_window_create()` blocks until the server has configured the window, and the size it
comes back with is the real one — clamped to at least 80×40, and to the screen. Always read
it back with `ui_window_width()` / `ui_window_height()` rather than assuming you got what
you asked for. In practice you never need to: the layout callback below is handed the size.

Titles are truncated to `UI_TITLE_MAX` (64) bytes including the terminator.

## 5. The view and the widgets

A `ui_view_t` wraps the window's pixel buffer in a cairo surface and owns a list of
widgets:

```c
ui_view_t* view = ui_view_create(window);
```

Widgets are created against the view and are freed with it. They are appended in creation
order, which is paint order: a widget created later draws over one created earlier, and hit
testing takes the last match, so the thing on top is the thing you click.

```c
static int hello_build(ui_view_t* view) {

    if ((hello_card = ui_panel_create(view)) == NULL) {
        return -1;
    }

    ui_panel_set_color(hello_card, ui_theme()->surface_sunken);
    ui_panel_set_border(hello_card, ui_theme()->border, 1.0);


    if ((hello_value = ui_label_create(view, "0")) == NULL) {
        return -1;
    }

    ui_label_set_align(hello_value, UI_ALIGN_CENTER);


    if ((hello_reset = ui_button_create(view, "Reset", hello_on_reset, NULL)) == NULL) {
        return -1;
    }

    if ((hello_add = ui_button_create(view, "+1", hello_on_add, NULL)) == NULL) {
        return -1;
    }

    ui_button_set_style(hello_add, UI_BUTTON_STYLE_PRIMARY);

    return 0;
}
```

Nothing here says where anything goes. Widgets are created with an empty rectangle and stay
invisible until something places them, which is the layout callback's job.

Note that the panel is created first and the label second: the label is transparent and
draws over the panel, which is how a caption on a surface is expressed. There is no
parent-child relationship — a label is not *inside* a panel, it is merely drawn after it and
positioned within it.

## 6. Layout

```c
static void hello_layout(ui_view_t* view, int width, int height, void* user) {

    (void)view;
    (void)user;


    ui_rect_t content = {

        .x      = HELLO_MARGIN,
        .y      = HELLO_MARGIN,
        .width  = width - 2 * HELLO_MARGIN,
        .height = height - 2 * HELLO_MARGIN,
    };

    if (content.width <= 0 || content.height <= HELLO_ROW + HELLO_GAP) {
        return;
    }


    ui_rect_t card = {

        .x      = content.x,
        .y      = content.y,
        .width  = content.width,
        .height = content.height - HELLO_ROW - HELLO_GAP,
    };

    ui_widget_place(hello_card, card);
    ui_widget_place(hello_value, ui_rect_inset(card, 12));

    /* Type scales with the box it sits in rather than with the window, so the number keeps
       its relationship to the panel around it at any size. */
    ui_label_set_font(hello_value, UI_FONT_BOLD, card.height * 0.35);


    ui_rect_t buttons = {

        .x      = content.x,
        .y      = content.y + content.height - HELLO_ROW,
        .width  = content.width,
        .height = HELLO_ROW,
    };

    ui_grid_t grid = ui_grid(buttons, 2, 1, HELLO_GAP);

    ui_widget_place(hello_reset, ui_grid_cell(&grid, 0, 0, 1, 1));
    ui_widget_place(hello_add, ui_grid_cell(&grid, 1, 0, 1, 1));
}
```

The callback runs once when it is installed and again after every resize the view has
applied, with the content size already in place. Everything positional belongs here: a
window that can be dragged to a different size has no fixed geometry to hardcode anywhere
else.

The early return matters. A window dragged small enough produces negative dimensions, and
laying out against those puts widgets in places arithmetic invented. Returning leaves them
where they were, which is wrong but bounded.

`ui_grid()` and `ui_grid_cell()` are a convenience over the arithmetic, not a layout
engine — they compute rectangles and you place widgets in them. See
[tutorial-layout.md](tutorial-layout.md) for what they do about remainder pixels and spans.

## 7. State changes

The callbacks change state and then say what has to be redrawn:

```c
static void hello_refresh(void) {

    char text[32];

    snprintf(text, sizeof(text), "%d", hello_count);

    ui_label_set_text(hello_value, text);

    /* Nothing to reset at zero, and a control that can do nothing should say so rather
       than look ready and then ignore the press. */
    ui_widget_set_enabled(hello_reset, hello_count != 0);
}


static void hello_on_add(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    hello_count++;

    hello_refresh();
}
```

There is no explicit repaint call. Every setter that changes something visible invalidates
the widget's rectangle, and setters that change nothing — `ui_label_set_text()` with the
string already on screen, `ui_widget_set_enabled()` with the state it already has — return
without invalidating anything. A `hello_refresh()` called on every frame with nothing new to
say therefore costs nothing.

A disabled button is dimmed towards the background and drops out of hit testing entirely,
including any hover or press it was holding at the moment it was disabled.

## 8. The run loop

```c
    if (hello_build(view) < 0) {
        /* ... */
    }

    /* Installed last, because it runs immediately and everything it places has to exist. */
    ui_view_on_layout(view, hello_layout, NULL);

    hello_refresh();

    const int status = ui_view_run(view);
```

`ui_view_run()` is: paint whatever is dirty, commit it, block for an event, dispatch it,
repeat, until the window is closed or the server goes away. It returns `0` on a clean close
and `-1` otherwise, with `errno` set.

Order matters at startup. `ui_view_on_layout()` runs the callback straight away — widgets
created before it have no geometry, and everything downstream assumes a laid-out tree — so
install it after the widgets exist.

If you need your own loop (another descriptor to poll, a timer, a second window), call
`ui_view_present()` and `ui_view_dispatch()` yourself; `ui_view_run()` is nothing more than
those two around `ui_next_event()`. See [widgets.md](widgets.md#driving-your-own-loop).

## 9. Teardown

```c
    ui_view_destroy(view);
    ui_window_destroy(window);
    ui_disconnect(conn);
```

In that order: the view holds a cairo surface pointing into the window's pixels, and the
window holds a reference the connection cleans up. `ui_disconnect()` frees any windows still
attached, so a program exiting on an error path can skip straight to it.

## 10. The whole thing

`apps/extra/ui-hello/main.c`:

```c
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aplus/ui-widgets.h>
#include <aplus/ui.h>


#define HELLO_MARGIN 16
#define HELLO_GAP    10
#define HELLO_ROW    40


static ui_widget_t* hello_card;
static ui_widget_t* hello_value;
static ui_widget_t* hello_add;
static ui_widget_t* hello_reset;

static int hello_count;


static void hello_refresh(void) {

    char text[32];

    snprintf(text, sizeof(text), "%d", hello_count);

    ui_label_set_text(hello_value, text);

    /* Nothing to reset at zero, and a control that can do nothing should say so rather
       than look ready and then ignore the press. */
    ui_widget_set_enabled(hello_reset, hello_count != 0);
}


static void hello_on_add(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    hello_count++;

    hello_refresh();
}


static void hello_on_reset(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    hello_count = 0;

    hello_refresh();
}


/* Everything positional, run once at startup and again after every resize. A window that
 * can be dragged to a different size has no fixed geometry to hardcode anywhere else.
 */
static void hello_layout(ui_view_t* view, int width, int height, void* user) {

    (void)view;
    (void)user;


    ui_rect_t content = {

        .x      = HELLO_MARGIN,
        .y      = HELLO_MARGIN,
        .width  = width - 2 * HELLO_MARGIN,
        .height = height - 2 * HELLO_MARGIN,
    };

    if (content.width <= 0 || content.height <= HELLO_ROW + HELLO_GAP) {
        return;
    }


    ui_rect_t card = {

        .x      = content.x,
        .y      = content.y,
        .width  = content.width,
        .height = content.height - HELLO_ROW - HELLO_GAP,
    };

    ui_widget_place(hello_card, card);
    ui_widget_place(hello_value, ui_rect_inset(card, 12));

    /* Type scales with the box it sits in rather than with the window, so the number keeps
       its relationship to the panel around it at any size. */
    ui_label_set_font(hello_value, UI_FONT_BOLD, card.height * 0.35);


    ui_rect_t buttons = {

        .x      = content.x,
        .y      = content.y + content.height - HELLO_ROW,
        .width  = content.width,
        .height = HELLO_ROW,
    };

    ui_grid_t grid = ui_grid(buttons, 2, 1, HELLO_GAP);

    ui_widget_place(hello_reset, ui_grid_cell(&grid, 0, 0, 1, 1));
    ui_widget_place(hello_add, ui_grid_cell(&grid, 1, 0, 1, 1));
}


static int hello_build(ui_view_t* view) {

    if ((hello_card = ui_panel_create(view)) == NULL) {
        return -1;
    }

    ui_panel_set_color(hello_card, ui_theme()->surface_sunken);
    ui_panel_set_border(hello_card, ui_theme()->border, 1.0);


    if ((hello_value = ui_label_create(view, "0")) == NULL) {
        return -1;
    }

    ui_label_set_align(hello_value, UI_ALIGN_CENTER);


    if ((hello_reset = ui_button_create(view, "Reset", hello_on_reset, NULL)) == NULL) {
        return -1;
    }

    if ((hello_add = ui_button_create(view, "+1", hello_on_add, NULL)) == NULL) {
        return -1;
    }

    ui_button_set_style(hello_add, UI_BUTTON_STYLE_PRIMARY);

    return 0;
}


int main(int argc, char** argv) {

    (void)argc;
    (void)argv;


    ui_connection_t* conn = ui_connect(NULL, 5000);

    if (!conn) {
        fprintf(stderr, "ui-hello: ui_connect() failed: %s\n", strerror(errno));
        return 1;
    }


    ui_window_t* window = ui_window_create(conn, 260, 200, "ui-hello");

    if (!window) {
        fprintf(stderr, "ui-hello: ui_window_create() failed: %s\n", strerror(errno));
        ui_disconnect(conn);
        return 1;
    }


    ui_view_t* view = ui_view_create(window);

    if (!view) {
        fprintf(stderr, "ui-hello: ui_view_create() failed: %s\n", strerror(errno));
        ui_window_destroy(window);
        ui_disconnect(conn);
        return 1;
    }


    if (hello_build(view) < 0) {
        fprintf(stderr, "ui-hello: cannot create the widgets\n");
        ui_view_destroy(view);
        ui_window_destroy(window);
        ui_disconnect(conn);
        return 1;
    }

    /* Installed last, because it runs immediately and everything it places has to exist. */
    ui_view_on_layout(view, hello_layout, NULL);

    hello_refresh();


    const int status = ui_view_run(view);

    if (status < 0) {
        fprintf(stderr, "ui-hello: ui_view_run() failed: %s\n", strerror(errno));
    }

    ui_view_destroy(view);
    ui_window_destroy(window);
    ui_disconnect(conn);

    return status < 0 ? 1 : 0;
}
```

## 11. Build and run

```bash
./makew all
```

```bash
./makew run
```

In the terminal the window manager opens for you:

```sh
ui-hello
```

The window appears where the server decides to put it, with decorations around it. Drag an
edge and watch the number resize with the panel; the layout callback is running on every
configure.

`Ctrl+Alt+T` opens another terminal, `Ctrl+Alt+Q` closes the focused window. Closing from
the titlebar button sends `UI_EVENT_CLOSE`, which `ui_view_run()` turns into a clean return
from `main()` — the application decides when to die, not the server.

For an automated run with the console captured instead of a display, `./makew run-headless`.

## Where to go next

- The keyboard, spanning grid cells, and a layout that adapts: [tutorial-layout.md](tutorial-layout.md).
- Drawing something the widgets cannot express: [tutorial-custom-drawing.md](tutorial-custom-drawing.md).
- The full widget reference: [widgets.md](widgets.md).
