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

`ui_view_on_key()` installs the window's key handling. It sees every key the focused widget
did not take — see [Keyboard focus](#keyboard-focus) — and every key at all when nothing is
focused, which is the state a view starts in. `vkey` is a raw `KEY_*` code from
`<aplus/input.h>`; there is no keymap anywhere in the system, so characters, modifiers and
repeat are the application's business. Both are covered in
[tutorial-layout.md](tutorial-layout.md#the-keyboard).

### Keyboard focus

```c
bool          ui_view_focus(ui_view_t* view, ui_widget_t* widget);
ui_widget_t*  ui_view_focused(const ui_view_t* view);
```

One widget in a view can hold the keyboard. Keys reach it before the view's key callback, and
fall through to the callback only when it declines them — a list that ignores a letter leaves
the letter to the application, and a list that takes `KEY_DOWN` keeps it.

`ui_view_focus(view, NULL)` focuses nothing, which sends every key to the callback again. A
widget that is hidden or disabled is refused the focus rather than taking it silently, and one
hidden or disabled *while* focused gives it up on the spot — so a layout callback that hides
half the window as it narrows leaves the keyboard nowhere rather than in something invisible.
Where it should go instead is the application's to say. The return value says whether anything
has to be repainted, which the view has already arranged; it is not an error indication.

The focus also moves on its own: a press focuses the widget it lands on, but only one that
takes keys at all. Clicking a button therefore does not take the focus away from a list, which
is what lets a toolbar sit next to a list without disarming the arrow keys every time it is
used.

`UI_EVENT_FOCUS` — the window gaining or losing the keyboard from the server — does not
disturb this. The widget that held the focus still holds it when the window comes back.

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

Every invalidated rectangle is grown by one pixel on every side before it is recorded —
widgets are drawn with anti-aliased edges, so the pixels a widget affects reach a hair past
the rectangle it claims, and without the margin the outermost row of a rounded corner gets
left behind.

The rectangles accumulate into a set of up to eight rather than the one box around them all,
merging into each other only where merging is the cheaper of the two —
[window-api.md](window-api.md#damage-and-commits) has the rule. Two widgets at opposite
corners of a window are therefore two rectangles, and the space between them is not repainted
at all.

`ui_view_present()` then, for that set: clips to all of it at once, paints the theme
background across the clip, draws every visible widget that intersects any rectangle in it in
paint order, flushes, damages the window with each rectangle and commits. The background pass
is why a label that got shorter does not leave the tail of the old string behind, and the clip
is why it costs the damaged rectangles rather than the whole window.

| Return | |
|---|---|
| `1` | A frame was painted and committed |
| `0` | Nothing needed painting |
| `-1` | Error, `errno` set |

`ui_view_run()` is the loop: present, block for an event, dispatch, drain whatever else is
already queued, repeat, until the view is closed or the connection fails. It returns `0` on a
clean close, `-1` otherwise.

The drain is what keeps a burst of pointer motion from costing a repaint and a commit each —
and a whole composite in the server for each of those. It is bounded at 256 events, so a
server talking faster than this end can draw slows the painting down rather than starving it
altogether.

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
| `UI_EVENT_POINTER` | Hit tests, updates hover and press state, moves the keyboard focus on a press, counts double clicks, runs a click action on release |
| `UI_EVENT_SCROLL` | Goes to the widget under the pointer, and nowhere if that widget does not scroll |
| `UI_EVENT_LEAVE` | Clears hover and press — the release for a press that ended outside the window never arrives |
| `UI_EVENT_FOCUS` | On focus loss, clears hover and press. Gaining focus says nothing about where the pointer is; the `UI_EVENT_POINTER` that follows does. The keyboard focus *inside* the view is not touched |
| `UI_EVENT_KEY` | Offered to the focused widget first, then to the key callback if the widget declined it |
| `UI_EVENT_CLOSE` | Sets the closed flag |

Because a configure is applied here, a caller driving its own loop does not also have to
call `ui_window_apply_configure()` — doing both is harmless (the second finds nothing
pending) but pointless.

`ui_view_dispatch()` does not look at `event.window_id`. One view assumes one window; a
process with two windows needs its own loop that routes events to the right view by id.

### Driving your own loop

`ui_view_run()` is this plus the drain, and replacing it is expected as soon as there is a
second descriptor to watch:

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

Keep dispatching while `ui_next_event(conn, &event, 0)` still returns events before going back
to `ui_view_present()`, bounded by some count of your own. A window being dragged across
produces a pointer event per mouse packet, and painting each of them is a commit and a
composite for a frame nobody sees.

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
out of hit testing. `ui_widget_set_visible(widget, false)` takes it out of painting as well.

Both give up whatever the widget was holding in the view at that moment — the hover, the
press and the keyboard focus, each announced to the widget as it goes. Otherwise a control
that went away under the pointer keeps a hover it can no longer act on, one that went away
mid-press stays pressed forever, and one that was focused goes on eating every key the window
receives while being impossible to see or click.

Neither is restored by turning the widget back on. A widget comes back visible or enabled and
holding nothing; the pointer moving over it hovers it again, and the keyboard has to be given
back deliberately with `ui_view_focus()`.

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

A press is held by the widget it landed on until the button comes back up, wherever the
pointer goes in between. Motion in that state is a drag, delivered to the pressing widget with
a flag saying whether the pointer is still inside it — which is how a scrollbar thumb keeps
following the pointer after it has slid off the list. The release goes to the same widget,
carrying the same flag.

A press also counts towards a double click: a second press on the same widget within 400 ms
and 4 pixels of the first. The count is reported to the widget on release, and reaching two
resets it, so a third click starts counting again rather than being a triple.

There is no parent-child relationship and no clipping between widgets. A label positioned
inside a panel's rectangle is not *in* the panel; it is merely drawn after it, in a place
that happens to overlap. Layout is absolute, everywhere.

Wheel events do not follow the press. They go to whatever the pointer is over, which is the
widget hover is already tracking.

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

## List

```c
ui_widget_t* ui_list_create(ui_view_t* view);

void ui_list_clear(ui_widget_t* widget);
int  ui_list_add(ui_widget_t* widget, const char* text, const char* detail, void* user);

size_t      ui_list_count(const ui_widget_t* widget);
const char* ui_list_text(const ui_widget_t* widget, int index);
void*       ui_list_item_user(const ui_widget_t* widget, int index);

int  ui_list_selected(const ui_widget_t* widget);
void ui_list_select(ui_widget_t* widget, int index);
void ui_list_scroll_to(ui_widget_t* widget, int index);

void ui_list_set_row_height(ui_widget_t* widget, int height);

void ui_list_on_select(ui_widget_t* widget, ui_list_fn fn, void* user);
void ui_list_on_activate(ui_widget_t* widget, ui_list_fn fn, void* user);
```

```c
typedef void (*ui_list_fn)(ui_widget_t* widget, int index, void* user);
```

One selectable row per item, in a sunken well with a rounded border, scrolled by the wheel,
the keyboard or its own scrollbar. This is the widget that takes keys, and the reason the view
has a focus at all.

A row is a name and an optional detail drawn against the right edge — a size column, a type, a
count. The detail is measured first and the name is ellipsised into what is left, so a long
name loses its tail rather than running under the number beside it.

| Property | Default |
|---|---|
| background | `theme->surface_sunken` |
| border | `theme->border`, or `theme->focus_ring` when focused |
| selected row | `theme->secondary` / `on_secondary`, or `theme->primary` / `on_primary` when focused |
| detail colour | `theme->text_muted`, or the row's own text colour when selected |
| row height | `theme->font_size * 2` — 28 at the default 14.0 |

`ui_list_add()` copies both strings and returns the new row's index, or `-1`. A caller's
buffer can be reused immediately; the row owns what it holds until `ui_list_clear()` or the
view's destruction. `user` is carried along and handed back by `ui_list_item_user()`, which is
where a file browser keeps whatever a row means — a full path, a struct, an index into
something else.

Rows are stored whole, however long, and the array behind them doubles as it fills. What is
capped is the drawing: a name is shaped at `UI_LIST_TEXT_MAX` (256) bytes, which is far past
the point where a row of any plausible width has been ellipsised anyway.

`ui_list_text()` returns a pointer into the row's own storage, valid until the list is cleared
or destroyed.

### Selection and activation

Selection and activation are separate, and a list reports them separately:

| | |
|---|---|
| `ui_list_on_select()` | The selected row changed — a click, an arrow key, `ui_list_select()` |
| `ui_list_on_activate()` | A double click on a row, or `Enter` on the selected one |

That split is the difference between showing what a row is and doing what a row is for: a file
browser fills in a status line from the first and opens the file from the second.

Both callbacks receive the row index, or `-1` when the selection was cleared. **Both run from
inside `ui_view_dispatch()`**, so a callback that rebuilds the list it was called from —
`ui_list_clear()` and a fresh set of rows, which is exactly what descending into a directory
is — is rebuilding a widget the dispatch is in the middle of. That is supported: the list
touches nothing of its own after calling out. What it does mean is that the index is only
meaningful before the rebuild.

Destroying the widget from its own callback is not. The view invalidates the widget it
dispatched to once the callback returns, and by then there is nothing there to invalidate.
Clear a list from its callback; free it from somewhere else.

`ui_list_select()` selects a row, scrolls it into view and runs the select callback — an index
out of range, `-1` included, clears the selection and runs the callback with `-1`. Selecting
the row already selected does nothing at all, callback included, so a refresh that re-asserts
the current selection is free. `ui_list_selected()` returns `-1` when nothing is selected,
which is what a list starts with.

A click selects the row under it; a double click on the row that is already selected activates
it. A click that lands on no row leaves the selection alone.

### Scrolling

The list scrolls in pixels, clamped so that the content never leaves the well. There is no
scrollbar widget: the bar is part of the list, appearing at 10 pixels wide down the right-hand
edge only when the rows do not fit. Dragging the thumb keeps the grab point inside it, so it
does not jump to the pointer when the drag starts, and clicking the track above or below the
thumb pages towards it. A drag of the thumb never activates a row, however it ends.

| Input | |
|---|---|
| Wheel | Three rows a detent |
| `Up` / `Down` | Move the selection a row, scrolling it into view |
| `Page Up` / `Page Down` | Move the selection a page |
| `Home` / `End` | First and last row |
| `Enter` | Activate the selected row |

A page is as many whole rows as the well holds. Only key *downs* are taken and only the keys
in that table: everything else, releases included, falls through to the view's key callback,
so an application can bind letters over a focused list without fighting it.

The keys need the focus, the wheel does not — it goes to whatever the pointer is over. A list
that has never been focused still scrolls under the pointer, it just does not move its
selection.

`ui_list_scroll_to()` scrolls a row into view without selecting it: nothing happens if it is
already visible, otherwise the list moves by the least that makes it so.

`ui_list_set_row_height()` overrides the height a row is given; `0` restores the height the
theme implies. Rows are a fixed height, which is what makes hit testing and the visible range
arithmetic rather than a walk — only the rows actually on screen are drawn, so a list of ten
thousand paints what a list of twenty paints.

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

The set is four widgets because four were needed, not because the shape resists a fifth. A
widget kind is a table of hooks — `ui_widget_ops_t` in `lib/aplus/ui/ui_widget_internal.h` —
and the view drives every kind through it, so a new one does not mean touching `ui_view.c` at
all:

```c
typedef struct {

    void (*draw)(ui_widget_t* widget, cairo_t* cr);

    bool (*on_hover)(ui_widget_t* widget, bool hovered);
    bool (*on_press)(ui_widget_t* widget, int x, int y);
    bool (*on_drag)(ui_widget_t* widget, int x, int y, bool inside);
    bool (*on_release)(ui_widget_t* widget, int x, int y, bool inside, int clicks);
    bool (*on_scroll)(ui_widget_t* widget, int delta);

    bool (*on_key)(ui_widget_t* widget, uint16_t vkey, bool down);
    bool (*on_focus)(ui_widget_t* widget, bool focused);

    void (*destroy)(ui_widget_t* widget);

} ui_widget_ops_t;
```

Every hook is optional — a panel fills in `draw` and nothing else. Every hook that reacts to
state returns whether a repaint is due, and the view invalidates the widget for it; `on_key`
returns whether the key was handled, which is what decides whether the application sees it.
Providing `on_key` at all is what makes a widget focusable by a click.

A new one is:

1. A `ui_widget_kind_t` in `ui_widget_internal.h`, and a member in the union there for its
   state.
2. A `ui_<kind>.c` with a static `ui_widget_ops_t`, a constructor calling
   `ui_widget_new(view, kind, &ops, interactive)`, the setters (each invalidating only when
   something actually changed), and the hooks themselves.
3. The public declarations in `lib/aplus/ui/include/aplus/ui-widgets.h`.

The setters are where the care goes: an unconditional `ui_widget_invalidate()` turns every
state refresh into a full repaint of that widget, and the library's whole approach to
efficiency is that nothing repaints unless it changed.

`destroy` is for a widget that owns memory — the list frees its rows there. Without it,
`ui_widget_destroy()` and the view's teardown free the widget and leak whatever it held.
