# Custom drawing

Three widgets do not cover everything. A plot, a game board, a hex dump, a video frame —
none of those is a panel with a label on it, and none of them should be. This covers the two
ways out: dropping to the raw surface entirely, and cutting a hole in a widget view and
drawing into it yourself.

## The surface underneath

Every window is a surface you can write to directly, view or no view:

```c
uint32_t* ui_window_pixels(ui_window_t* win);
int       ui_window_width(ui_window_t* win);
int       ui_window_height(ui_window_t* win);
size_t    ui_window_stride(ui_window_t* win);
```

| | |
|---|---|
| Format | `0xFFRRGGBB` — 8 bits per channel, not premultiplied, alpha ignored by the server |
| Stride | `ui_window_stride()` bytes per row — **not** `width * 4` |
| Origin | Top-left of the content area |

Those pixels are shared memory: the server composites straight out of them, so there is no
copy and no transfer, and whatever you leave there is what appears on screen.

Which is also why the stride matters. It is the one the server's cairo surface was built with,
usually padded past `width * 4`, and a client that assumes otherwise skews its image
progressively further down the window. Work a row at a time:

```c
uint8_t* base       = (uint8_t*)ui_window_pixels(win);
const size_t stride = ui_window_stride(win);

for (int y = 0; y < ui_window_height(win); y++) {

    uint32_t* row = (uint32_t*)(base + (size_t)y * stride);

    for (int x = 0; x < ui_window_width(win); x++) {
        row[x] = 0xFF000000U | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
}
```

`apps/test/ui-test/main.c` paints its gradient exactly like that.

Write whatever you like into it, then say what changed and send it:

```c
void ui_window_damage(ui_window_t* win, int x, int y, int width, int height);
void ui_window_damage_all(ui_window_t* win);
int  ui_window_commit(ui_window_t* win);
```

Damage accumulates into a set of up to eight rectangles, which merge into each other only
where merging is cheaper than keeping them apart — two far-apart corners stay two rectangles
rather than becoming the box around them (see
[window-api.md](window-api.md#damage-and-commits)). `ui_window_commit()` sends the set and
clears it, returning `0` with nothing sent when no damage is outstanding. Each message is a
rectangle and nothing else (see [protocol.md](protocol.md#commits)), so committing a whole
surface costs no more than committing one glyph.

Nothing reaches the screen without a commit. A frame that is drawn but never committed is a
frame the server never hears about.

## Doing without widgets entirely

`apps/test/ui-test/main.c` is the whole surface layer in one file: connect, create a window,
paint a gradient, and print every event. Its event loop is the shape every surface-layer
client has:

```c
    for (;;) {

        ui_event_t ev;

        int e = ui_next_event(conn, &ev, -1);

        if (e < 0) {
            break;
        }

        if (e == 0) {
            continue;
        }

        switch (ev.type) {

            case UI_EVENT_CONFIGURE:

                if (ui_window_apply_configure(win) < 0) {
                    break;
                }

                paint(win);
                ui_window_commit(win);

                break;

            case UI_EVENT_CLOSE:
                ui_window_destroy(win);
                ui_disconnect(conn);
                return 0;

            /* ... */
        }
    }
```

The part to get right is the configure. `ui_next_event()` records the new size but does not
act on it: reallocating the pixel buffer from the event path would free it under a thread
that is still drawing into it. `ui_window_apply_configure()` is where the resize actually
happens, so call it from wherever drawing is serialised.

Until you call it, the window keeps its old size, and commits carry the old serial — which
the server drops. That is the mechanism, not a bug: a resize racing an in-flight frame
becomes a discarded frame rather than an overrun. But a client that never applies a
configure never paints again, so this is the one event you cannot ignore.

After a successful apply, the buffer contents are gone (it is cleared, or reallocated) and
the whole surface is marked damaged. Repaint everything, not just what you think changed.

The full details are in [window-api.md](window-api.md).

## Cutting a hole in a view

Widgets and hand-drawn content can share one window. The pattern is: reserve a rectangle
that no widget occupies, and repaint it after the view has painted.

The ordering constraint is the whole trick. `ui_view_present()` lays down the theme
background across everything it is about to repaint before drawing widgets over it, so
anything of yours inside that region is erased. Your drawing therefore has to come *after*
the view's, and has to be redone whenever the view painted:

```c
    while (!ui_view_closed(canvas_view)) {

        const int painted = ui_view_present(canvas_view);

        if (painted < 0) {
            break;
        }

        /* The view repaints the background across everything it touched, so the canvas is
           gone whenever a frame went out and has to be laid down again on top. */
        if (painted > 0) {

            canvas_draw();

            if (ui_window_commit(canvas_window) < 0) {
                break;
            }
        }


        ui_event_t event;

        const int e = ui_next_event(conn, &event, -1);

        if (e < 0) {
            break;
        }

        if (e == 0) {
            continue;
        }

        ui_view_dispatch(canvas_view, &event);
    }
```

This is `ui_view_run()` with one extra step, which is why it is written out by hand: the
built-in loop knows about widgets and nothing else.

Redrawing the canvas whenever *any* frame went out is the simple version — a button lighting
up under the pointer costs a canvas repaint it did not need. If that matters, compare the
view's damage against your rectangle and skip when they do not overlap; `ui_view_present()`
does not report the region it painted, so you would track it yourself from what you
invalidated.

To make the view repaint at all — because the widgets themselves have not changed, only your
content has — invalidate through the view:

```c
static void canvas_on_step(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    canvas_phase += 0.25;

    /* The view has no idea the canvas exists, so the redraw has to be asked for through
       the rectangle it does know about. */
    ui_view_invalidate(canvas_view);
}
```

`ui_view_invalidate()` marks the whole view dirty; `ui_widget_invalidate()` marks one
widget's rectangle. Either makes `ui_view_needs_paint()` true and the next
`ui_view_present()` return `1`.

## Your own cairo context

Cairo is already linked into any application using widgets, so the easy way to draw into
the hole is a context of your own over the same pixels:

```c
static void canvas_draw(void) {

    if (canvas_area.width <= 0 || canvas_area.height <= 0) {
        return;
    }


    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        (unsigned char*)ui_window_pixels(canvas_window),
        CAIRO_FORMAT_RGB24,
        ui_window_width(canvas_window),
        ui_window_height(canvas_window),
        (int)ui_window_stride(canvas_window));

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return;
    }


    cairo_t* cr = cairo_create(surface);

    /* Inside the frame rather than over it: the panel's border is drawn by the view, and a
       canvas clipped to the same rectangle would paint across it. */
    const ui_rect_t inner = ui_rect_inset(canvas_area, 2);

    cairo_rectangle(cr, inner.x, inner.y, inner.width, inner.height);
    cairo_clip(cr);

    /* ... draw ... */

    cairo_destroy(cr);

    cairo_surface_flush(surface);
    cairo_surface_destroy(surface);


    ui_window_damage(canvas_window, canvas_area.x, canvas_area.y, canvas_area.width, canvas_area.height);
}
```

Four things in there are not optional:

**`CAIRO_FORMAT_RGB24`, not `ARGB32`.** The memory layout of the two is identical, so the
wrong one still draws; what differs is that cairo treats ARGB32 as premultiplied and the
protocol carries plain `0xFFRRGGBB`. Choosing ARGB32 makes cairo un-premultiply pixels that
were never premultiplied, and the colours come out subtly wrong in a way that only shows up
where alpha is involved.

**Clip to your rectangle.** Nothing else stops a stray path from painting over a widget.

**`cairo_surface_flush()` before the commit.** The surface writes straight into the window's
pixels, but cairo is free to hold some of them back until it is told the drawing has
finished. Commit without flushing and you send a partially-drawn frame.

**Create the surface from `ui_window_pixels()` each time, or rebind after every configure.**
A resize can hand out a different buffer, so a cached surface can end up pointing at freed
memory. Creating it per frame, as above, costs an allocation and cannot get this wrong;
caching it means rebinding in your configure path, exactly as `ui_view_bind_surface()` does
internally.

## Text without widgets

`ui_draw_text()` and the font cache are internal to the library — `ui_widget_internal.h` is
not installed — so hand-drawn text goes through cairo directly. The theme still tells you
which file to open:

```c
    cairo_font_face_t* face = /* your own cairo_ft_font_face_create_for_ft_face() */;

    cairo_set_font_size(cr, ui_theme()->font_size);
```

`ui_theme()->font_regular` and `->font_bold` are absolute paths, because there is no
fontconfig in the sysroot for a family name to resolve against. Load them with FreeType and
wrap them with `cairo_ft_font_face_create_for_ft_face()`; cairo's toy font API
(`cairo_select_font_face()`) has nothing to select from here.

Cache whatever face you create. A toolkit asks for one on every string it draws, and
reopening the file that often dominates the cost of painting.

## Choosing between the two

| | Raw surface | Hole in a view |
|---|---|---|
| Whole window is your content (a terminal, a game, a video) | ✔ | |
| Content plus ordinary controls around it | | ✔ |
| Nothing from cairo, no fonts | ✔ | |
| You want the theme, and buttons that behave | | ✔ |

`apps/sysutils/aplus-terminal` is the first kind: it owns every pixel and has no use for a
button. Most things that are not a terminal are the second, and `apps/sysutils/aplus-image-viewer`
is the worked example of it: a toolbar of buttons and labels the view paints, a panel whose
inside the layout callback records, and a cairo context over `ui_window_pixels()` that draws the
picture into it and damages that rectangle alone.
