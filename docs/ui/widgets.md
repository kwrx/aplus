# Widgets

Reference for `<aplus/ui-widgets.h>`. For a worked introduction, start with
[getting-started.md](getting-started.md).

The widget layer is built entirely on the public surface API. Everything here could be
written by an application against `<aplus/ui.h>` alone; none of it is privileged.

## The view

```c
ui_view_t* ui_view_create(ui_window_t* window);
void       ui_view_destroy(ui_view_t* view);

ui_window_t* ui_view_window(ui_view_t* view);
```

A view owns three things: a cairo surface bound over the window's pixel buffer, a list of
widgets, and a record of what has changed since the last frame.

It also takes a copy of the current theme pointer when it is created. That snapshot is
deliberate — it means a window cannot change appearance underneath itself because something
else swapped the global theme — and it means `ui_theme_set()` has to be called *before*
`ui_view_create()` to have any effect. See [theming.md](theming.md).

`ui_view_destroy()` frees every widget in the view. Widget pointers are valid until then, or
until the widget is explicitly destroyed.

Cairo enters your link here. A program that never calls `ui_view_create()` never pulls in
`ui_view.o`, and `-lui` on its own is enough to link it.

### Callbacks

```c
typedef void (*ui_layout_fn)(ui_view_t* view, int width, int height, void* user);
typedef bool (*ui_key_fn)(ui_view_t* view, uint16_t vkey, bool down, void* user);

void ui_view_on_layout(ui_view_t* view, ui_layout_fn fn, void* user);
void ui_view_on_key(ui_view_t* view, ui_key_fn fn, void* user);
```

`ui_view_on_layout()` runs the callback immediately, then again after every configure the
view applies. Install it after creating the widgets it places.

`ui_view_on_key()` installs the only key handling a view has. `vkey` is a raw `KEY_*` code
from `<aplus/input.h>`; there is no keymap anywhere in the system, so characters, modifiers
and repeat are the application's business. Both are covered in
[tutorial-layout.md](tutorial-layout.md#the-keyboard).

### The frame

```c
void ui_view_invalidate(ui_view_t* view);
bool ui_view_needs_paint(ui_view_t* view);
int  ui_view_present(ui_view_t* view);
int  ui_view_run(ui_view_t* view);
bool ui_view_closed(ui_view_t* view);
```

A frame is: something invalidates a rectangle, `ui_view_present()` repaints that rectangle
and commits it.

Invalidated rectangles accumulate into a single bounding box, grown by one pixel on every
side — widgets are drawn with anti-aliased edges, so the pixels a widget affects reach a hair
past the rectangle it claims, and without the margin the outermost row of a rounded corner
gets left behind.

`ui_view_present()` then, for that box: clips to it, paints the theme background across it,
draws every visible widget that intersects it in paint order, flushes, damages the window
and commits. The background pass is why a label that got shorter does not leave the tail of
the old string behind, and the clip is why it costs the damaged rectangle rather than the
whole window.

| Return | |
|---|---|
| `1` | A frame was painted and committed |
| `0` | Nothing needed painting |
| `-1` | Error, `errno` set |

`ui_view_run()` is the loop: present, block for an event, dispatch, repeat, until the view
is closed or the connection fails. It returns `0` on a clean close, `-1` otherwise.

`ui_view_closed()` becomes true when a `UI_EVENT_CLOSE` is dispatched — the titlebar button,
`Ctrl+Alt+Q`, or anything else the server decides is a close request. Nothing in the library
exits on your behalf; a close is a message, and an application with unsaved state is free to
ignore it.

### Dispatch

```c
bool ui_view_dispatch(ui_view_t* view, const ui_event_t* event);
```

Feeds one event into the widgets, returning true when the view made something of it. What it
does with each type:

| Event | |
|---|---|
| `UI_EVENT_CONFIGURE` | Calls `ui_window_apply_configure()`, rebinds the cairo surface, re-runs the layout callback, invalidates everything |
| `UI_EVENT_POINTER` | Hit tests, updates hover and press state, runs a click action on release |
| `UI_EVENT_LEAVE` | Clears hover and press — the release for a press that ended outside the window never arrives |
| `UI_EVENT_FOCUS` | On focus loss, clears hover and press. Gaining focus says nothing about where the pointer is; the `UI_EVENT_POINTER` that follows does |
| `UI_EVENT_KEY` | Forwarded to the key callback, if one is installed |
| `UI_EVENT_CLOSE` | Sets the closed flag |

Because a configure is applied here, a caller driving its own loop does not also have to
call `ui_window_apply_configure()` — doing both is harmless (the second finds nothing
pending) but pointless.

`ui_view_dispatch()` does not look at `event.window_id`. One view assumes one window; a
process with two windows needs its own loop that routes events to the right view by id.

### Driving your own loop

`ui_view_run()` is exactly this, and replacing it is expected as soon as there is a second
descriptor to watch:

```c
    while (!ui_view_closed(view)) {

        if (ui_view_present(view) < 0) {
            break;
        }

        ui_event_t event;

        const int e = ui_next_event(conn, &event, -1);

        if (e < 0) {
            break;
        }

        if (e == 0) {
            continue;
        }

        ui_view_dispatch(view, &event);
    }
```

For a child process, a socket or a timer alongside the UI, poll `ui_connection_fd()` with
your own descriptors and call `ui_next_event(conn, &event, 0)` when it is readable. The
timeout rules are in [window-api.md](window-api.md#events).

Present before you block, not after you dispatch: an event that changes nothing leaves the
frame already on screen correct, and an application that paints after dispatching pays for
a repaint on every stray pointer move.

## Widgets

```c
void ui_widget_destroy(ui_widget_t* widget);

void      ui_widget_set_rect(ui_widget_t* widget, int x, int y, int width, int height);
void      ui_widget_place(ui_widget_t* widget, ui_rect_t rect);
ui_rect_t ui_widget_rect(const ui_widget_t* widget);

void ui_widget_set_visible(ui_widget_t* widget, bool visible);
bool ui_widget_visible(const ui_widget_t* widget);

void ui_widget_set_enabled(ui_widget_t* widget, bool enabled);
bool ui_widget_enabled(const ui_widget_t* widget);

void  ui_widget_set_user(ui_widget_t* widget, void* user);
void* ui_widget_user(const ui_widget_t* widget);

void ui_widget_invalidate(ui_widget_t* widget);
```

Widgets are created by their own constructors, belong to the view from the moment they
exist, and start visible, enabled, and with an empty rectangle — invisible in practice until
something places them.

Every setter that changes something visible invalidates the widget. Setters called with the
value already in place return without doing anything, which is what makes a refresh function
that rewrites the whole UI on every frame cost nothing when nothing changed.

`ui_widget_set_rect()` invalidates twice, before and after, because the rectangle being
vacated has to be repainted as well as the one being taken.

`ui_widget_set_enabled(widget, false)` dims the widget towards the background and takes it
out of hit testing. It also drops any hover or press it was holding at that moment —
otherwise a control disabled under the pointer keeps a hover it can no longer act on, and one
disabled mid-press stays pressed forever.

**`ui_widget_set_user()` on a button overwrites the argument its click callback receives.**
The constructor's `user` parameter and the widget's user pointer are the same slot. That is
usually what you want; it is a surprise if you expected two.

### Paint order and hit testing

The widget list is in creation order, and that is paint order: a widget created later draws
over one created earlier. Hit testing walks the same list to the end and keeps the *last*
match, so the topmost widget is the one that gets the pointer.

A widget is hit-testable only if it is visible, enabled, and interactive. Panels and labels
are not interactive at all, which is what lets them sit under a button without swallowing
its clicks. Only `UI_BUTTON_LEFT` drives anything.

There is no parent-child relationship and no clipping between widgets. A label positioned
inside a panel's rectangle is not *in* the panel; it is merely drawn after it, in a place
that happens to overlap. Layout is absolute, everywhere.

## Panel

```c
ui_widget_t* ui_panel_create(ui_view_t* view);
void ui_panel_set_color(ui_widget_t* widget, ui_color_t color);
void ui_panel_set_radius(ui_widget_t* widget, double radius);
void ui_panel_set_border(ui_widget_t* widget, ui_color_t color, double width);
```

A filled rounded rectangle, and the only widget that is purely background.

| Property | Default |
|---|---|
| colour | `theme->surface` |
| radius | `theme->corner_radius` (6.0) |
| border colour | `theme->border` |
| border width | `0.0` — no border |

The radius is clamped at half the shorter side, so "very round" is safe to ask for without
knowing how small a layout pass will make the widget. A border is stroked inset by half its
own width, because cairo centres a stroke on its path and half of a one-pixel border would
otherwise land outside the widget, over whatever the layout put next to it.

## Label

```c
ui_widget_t* ui_label_create(ui_view_t* view, const char* text);
void        ui_label_set_text(ui_widget_t* widget, const char* text);
const char* ui_label_text(const ui_widget_t* widget);
void        ui_label_set_align(ui_widget_t* widget, ui_align_t align);
void        ui_label_set_color(ui_widget_t* widget, ui_color_t color);
void        ui_label_set_font(ui_widget_t* widget, ui_font_weight_t weight, double size);
void        ui_label_set_padding(ui_widget_t* widget, int padding);
```

One line of text, clipped to its rectangle. Transparent: whatever is behind it shows
through, so a label over a panel needs no colour of its own.

| Property | Default |
|---|---|
| colour | `theme->text` |
| alignment | `UI_ALIGN_LEFT` |
| weight / size | `UI_FONT_REGULAR`, `theme->font_size` (14.0) |
| padding | `0` |

Text is stored in the widget, not referenced, and is capped at 127 bytes plus a terminator —
longer strings are truncated silently. `ui_label_text()` returns a pointer into that buffer,
valid until the next `ui_label_set_text()` or the view's destruction.

Horizontal placement follows the alignment; vertical placement centres on the *font's*
extents rather than the string's ink. Centring on the ink would make a row of buttons
reading `7 8 9 +` sit at four different heights, because none of those glyphs has the same
ink box.

`ui_label_set_font()` with a size of `0.0` or less resets to the theme's size rather than
drawing nothing.

There is no wrapping and no multi-line label. A paragraph is several labels and some
arithmetic, or your own cairo — see
[tutorial-custom-drawing.md](tutorial-custom-drawing.md).

## Button

```c
ui_widget_t* ui_button_create(ui_view_t* view, const char* text, ui_action_fn on_click, void* user);
void ui_button_set_text(ui_widget_t* widget, const char* text);
void ui_button_set_style(ui_widget_t* widget, ui_button_style_t style);
void ui_button_set_font(ui_widget_t* widget, ui_font_weight_t weight, double size);

void ui_button_activate(ui_widget_t* widget);
void ui_button_set_held(ui_widget_t* widget, bool held);
bool ui_button_held(const ui_widget_t* widget);
```

```c
typedef void (*ui_action_fn)(ui_widget_t* widget, void* user);
```

The callback receives the widget and the user pointer, which is enough for one function to
serve a keypad's worth of buttons — the calculator has twenty buttons and one callback,
distinguished by what each was given as `user`.

| Style | Fill | Text |
|---|---|---|
| `UI_BUTTON_STYLE_DEFAULT` | `theme->secondary` | `theme->on_secondary` |
| `UI_BUTTON_STYLE_PRIMARY` | `theme->primary` | `theme->on_primary` |
| `UI_BUTTON_STYLE_DANGER` | `theme->danger` | `theme->on_danger` |

Fill and text colour always travel together, so it is not possible to pick a legible-looking
fill and an illegible label to sit on it.

State is drawn as a single translucent wash over the style's fill, strongest first: pressed
or held uses `theme->active`, hover uses `theme->hover`, and a button both hovered and
pressed gets one wash rather than two — it should not read as twice as pressed. A disabled
button is faded towards the background instead.

Text is centred, capped at 127 bytes like a label's, and the label is always bold or regular
from the theme's two font files.

### Activation

An action runs on **release**, and only over the widget the press began on. Pressing a
button and sliding off it before letting go cancels the action, which is what makes a
mis-click recoverable; sliding back on re-arms it.

`ui_button_activate()` runs the action directly, as if the button had been clicked. It draws
nothing and does nothing when the button is disabled or hidden. `ui_button_set_held()` is
the other half: it draws the button as held without running anything. Together they make a
keyboard shortcut that is visible on screen, without duplicating what the button does — see
[tutorial-layout.md](tutorial-layout.md#binding-keys-to-buttons).

`held` is kept apart from the pointer's own pressed state on purpose: a key binding lights a
button with the pointer nowhere near it, and the two states must not cancel each other.

## Geometry helpers

```c
typedef struct {
    int x;
    int y;
    int width;
    int height;
} ui_rect_t;

ui_rect_t ui_rect_inset(ui_rect_t rect, int inset);

typedef struct {
    ui_rect_t bounds;
    int columns;
    int rows;
    int column_gap;
    int row_gap;
} ui_grid_t;

ui_grid_t ui_grid(ui_rect_t bounds, int columns, int rows, int gap);
ui_rect_t ui_grid_cell(const ui_grid_t* grid, int column, int row, int colspan, int rowspan);
```

Plain values that compute rectangles; they hold no widgets and nothing holds them. Build a
grid on the stack inside a layout callback and let it go.

`ui_grid()` clamps `columns` and `rows` to at least 1 and sets both gaps from the one
argument; assign `column_gap` and `row_gap` afterwards if they should differ.

`ui_grid_cell()` shares remainder pixels across tracks so the last one ends flush with the
bounds, and a span swallows the gaps it crosses so it lines up with the cells above and
below. An out-of-range cell returns `{0, 0, 0, 0}` rather than being clamped into a
neighbour's space. Worked examples in
[tutorial-layout.md](tutorial-layout.md#the-grid).

## Adding a widget

The set is three widgets because three were needed, not because the shape resists a fourth.
A new one is:

1. A `ui_widget_kind_t` in `lib/aplus/ui/ui_widget_internal.h`, and a member in the union
   there for its state.
2. A `ui_<kind>.c` with a constructor calling `ui_widget_new(view, kind, interactive)`, the
   setters (each invalidating only when something actually changed), and a
   `ui_<kind>_draw()`.
3. A case in `ui_widget_draw()`.
4. The public declarations in `lib/aplus/ui/include/aplus/ui-widgets.h`.

The setters are where the care goes: an unconditional `ui_widget_invalidate()` turns every
state refresh into a full repaint of that widget, and the library's whole approach to
efficiency is that nothing repaints unless it changed.

Anything needing pointer state beyond press and hover — a drag, a scroll, a caret — also needs
the view to track it, in `ui_view_pointer()`. Anything needing keyboard focus needs a
concept of focus, which the view does not currently have.
