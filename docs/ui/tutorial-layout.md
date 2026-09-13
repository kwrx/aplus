# Layout, resizing and the keyboard

[Getting started](getting-started.md) placed four widgets by hand. This goes further into
the two things that make a window feel finished rather than merely drawn: geometry that
survives being dragged to another size, and a keyboard that reaches the same controls the
pointer does.

The running example is `apps/sysutils/aplus-calculator/main.c`, which does both.

## The layout callback

```c
typedef void (*ui_layout_fn)(ui_view_t* view, int width, int height, void* user);

void ui_view_on_layout(ui_view_t* view, ui_layout_fn fn, void* user);
```

It runs in exactly two situations:

1. When you install it, immediately and synchronously. Widgets created after that point
   have no geometry until the next run, so create first and install second.
2. After the view has applied a configure — the new surface is already bound and `width`
   and `height` are the new content size.

There is no third. Nothing else in the library calls it, so if you change something that
affects geometry (a widget appearing, a font growing), either place the affected widgets
yourself or ask for a relayout by running the callback by hand.

The contract is that the callback is authoritative: it is expected to place *everything*,
every time, from the two numbers it is given. Placing half the widgets and leaving the rest
where they were works right up until the first resize.

### Guard the small end

```c
    if (content.width <= 0 || content.height <= HELLO_ROW + HELLO_GAP) {
        return;
    }
```

The server clamps windows at 80×40, which is small enough that a layout with margins and a
fixed toolbar row will arrive at negative dimensions. Rectangles computed from those are
not merely ugly, they are arbitrary: a negative width sends `x + width` back behind `x`, and
widgets end up in places arithmetic invented. Returning early leaves the previous geometry
in place — still wrong for the size, but bounded and stable.

## Rectangles

```c
typedef struct {
    int x;
    int y;
    int width;
    int height;
} ui_rect_t;

ui_rect_t ui_rect_inset(ui_rect_t rect, int inset);
```

`ui_rect_inset()` shrinks by `inset` on all four sides and clamps at empty rather than
going negative, which is what makes it safe to chain: padding inside padding inside a
window someone has dragged down to nothing still yields a valid rectangle.

Coordinates are content-relative and in pixels. `(0, 0)` is the top-left of the area you
own; the decorations are outside it and are not yours to think about.

## The grid

```c
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

A grid is a value, not an object: build one on the stack inside the layout callback, ask it
for rectangles, let it go. It holds no widgets and nothing holds it.

```c
    ui_grid_t grid = ui_grid(keypad, 4, 5, 8);

    for (size_t i = 0; i < sizeof(calc_keypad) / sizeof(calc_keypad[0]); i++) {

        ui_widget_t* button = calc.keys[calc_keypad[i].key];

        ui_widget_place(button, ui_grid_cell(&grid, calc_keypad[i].column, calc_keypad[i].row, 1, 1));
    }
```

Two properties are worth knowing because they are the ones that are tedious to get right by
hand:

**Remainder pixels are shared out, not dropped.** Tracks are computed by scaling the index
into the available space rather than by multiplying a rounded-down track size, so
consecutive tracks differ by at most one pixel and the last one ends exactly on the far
edge. A 4-column grid over 101 pixels gives you cells of 23, 23, 23 and 24 — not four cells
of 23 and a 9-pixel gap at the right where the layout quietly failed to reach.

**Spans swallow the gaps they cross.** A cell with `colspan = 2` is as wide as two cells
plus the gap between them, so it lines up exactly with the cells above and below it:

```c
    /* A double-width zero key, as on a pocket calculator. */
    ui_widget_place(calc.keys[CALC_KEY_0], ui_grid_cell(&grid, 0, 4, 2, 1));
```

A cell asked for outside the grid — a negative index, or a span that runs off the end —
comes back as `{0, 0, 0, 0}` rather than clamped into a neighbour's space, where it would
silently overlap whatever is already there. An empty rectangle is visible as a missing
widget; an overlapping one looks like a drawing bug.

## Sizing type with the box

A window that resizes and keeps a fixed font size looks wrong at both ends. Scale from the
rectangle rather than from the window, so that a control keeps its relationship to the box
it sits in:

```c
    double size = ui_grid_cell(&grid, 0, 0, 1, 1).height * 0.44;

    if (size < 11.0) {
        size = 11.0;
    }

    if (size > 26.0) {
        size = 26.0;
    }

    ui_button_set_font(button, UI_FONT_BOLD, size);
```

The clamps are not optional. Text is clipped to its widget's rectangle, so an unclamped
size on a small window produces buttons with a sliver of a glyph in them; at the other end
it produces a title where a label was wanted.

## Showing and hiding

An adaptive layout usually means showing fewer things rather than shrinking everything:

```c
    const bool roomy = width >= 420;

    ui_widget_set_visible(app_sidebar, roomy);
    ui_widget_set_visible(app_sidebar_title, roomy);

    ui_rect_t main = roomy ? ui_grid_cell(&grid, 1, 0, 2, 1) : ui_grid_cell(&grid, 0, 0, 3, 1);
```

`ui_widget_set_visible(widget, false)` takes the widget out of both painting and hit
testing and invalidates the rectangle it was occupying, so what was behind it is repainted.
It does not move it: the widget keeps its geometry and comes back where it was.

Placing a hidden widget is harmless, so the simplest correct layout callback places
everything unconditionally and only toggles visibility.

## The keyboard

```c
typedef bool (*ui_key_fn)(ui_view_t* view, uint16_t vkey, bool down, void* user);

void ui_view_on_key(ui_view_t* view, ui_key_fn fn, void* user);
```

One callback for the whole window. There is no keyboard focus between widgets and no
built-in key handling anywhere in the view, so without this callback a window is
pointer-only.

`vkey` is a raw `KEY_*` code from `<aplus/input.h>`, exactly as it came out of `/dev/kbd`.
The server does not own a keymap — translation stays wherever the characters are actually
needed — which has three consequences:

- **Layout is yours.** `KEY_1` is the key in that physical position, not the character `1`.
- **Modifiers are yours.** Shift arrives as `KEY_LEFTSHIFT` going down and up like any
  other key. Track it:

  ```c
      if (vkey == KEY_LEFTSHIFT || vkey == KEY_RIGHTSHIFT) {
          calc.shift = down;
          return true;
      }
  ```

- **Repeat is yours to recognise.** A held key produces a stream of downs with no
  intervening up.

Only the focused window receives keys, and focus changes arrive as `UI_EVENT_FOCUS`. If you
track modifier state, clear it when focus is lost — the release that would have cleared it
goes to whoever has focus now, and a shift stuck down outlives the window that saw it
pressed.

Return `true` when you handled the key. Nothing in the library acts on the return value
today; it is there so that a future view-level binding can tell whether the application
already claimed the key.

### Binding keys to buttons

The temptation is to give the key handler its own copy of what the button does. Don't:
route it through the button instead, and the two paths cannot drift apart.

```c
ui_widget_t* button = calc.keys[key];

if (down) {

    /* A held key repeats, and each repeat arrives as another press. Acting only on the
       first is what keeps leaning on "5" from filling the display with fives. */
    if (ui_button_held(button)) {
        return true;
    }

    ui_button_set_held(button, true);
    ui_button_activate(button);

} else {

    ui_button_set_held(button, false);
}
```

Three functions, with distinct jobs:

| | |
|---|---|
| `ui_button_activate()` | Runs the click action. Does not draw anything, and does nothing if the button is disabled or hidden. |
| `ui_button_set_held()` | Draws the button as held. Runs nothing. |
| `ui_button_held()` | Whether it is currently drawn held — which is also a usable "is this key already down" flag, as above. |

`held` is deliberately separate from the pointer's own pressed state, so a key binding
lighting a button cannot cancel a press happening under the pointer at the same moment, and
the two cannot both be released by one of them ending.

## Pointer details worth knowing

The view handles the pointer for you, but the rules it implements are worth stating because
they are the ones users notice:

- Only the **left** button drives widgets. `UI_BUTTON_RIGHT` and `UI_BUTTON_MIDDLE` arrive
  in the event and are ignored by the widget layer; reach them through your own dispatch if
  you need them.
- An action runs on **release**, and only over the widget the press began on. Sliding off a
  button before letting go cancels it, which is what makes a mis-click recoverable.
- A press that ends outside the window never arrives as a release. `UI_EVENT_LEAVE` is what
  drops it, and the view handles that too — this is the event that exists precisely because
  leaving is the one pointer transition that produces nothing else.

## Checklist

- Create widgets, then install the layout callback.
- Place everything from `width` and `height`, every time.
- Guard against a window too small to lay out.
- Clamp any font size derived from geometry.
- Track modifiers yourself, and clear them on focus loss.
- Bind keys to buttons, not to the actions behind them.
