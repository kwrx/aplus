# Components by example

[Getting started](getting-started.md) built a window out of a panel, a label and two buttons.
This is the rest of the set, used the way the applications in `apps/sysutils` use it: what each
widget is for, the handful of patterns that come up every time it is used, and the traps each
one has. It ends with a complete application, `ui-places`, that puts all five widgets in one
window — a search field filtering a list of places, a card describing the selected one, and
buttons that act on it.

Everything here is the public API of `<aplus/ui-widgets.h>`, plus two text helpers from
`<aplus/ui-draw.h>`. The full contract of every call is in [widgets.md](widgets.md); this page
is about putting them together.

## Which widget for what

| Widget | Use it for | Takes the pointer | Takes keys |
|---|---|---|---|
| Panel | Backgrounds: cards, wells, separators, a backdrop | No | No |
| Label | One line of text: headings, values, captions, an empty state | No | No |
| Button | Anything that does something when pressed, with text, an icon or both | Yes | No — bind keys to it yourself |
| List | Choosing one of many rows, each carrying a pointer of your own | Yes | Yes, once focused |
| Entry | One line of typed text: a search field, a name, a prompt | Yes | Yes, once focused |

Two of them are not interactive at all, and that matters as much as what the others do: a panel
or a label can sit anywhere, over or under anything, without stealing a click. That is what
makes the composition patterns below work, since there are no containers — every widget is
placed in window coordinates and drawn in the order it was created.

## Panels

A panel is a rounded rectangle and nothing else. Its uses are all variations on that.

**A card** — a sunken region with a border, which the labels and buttons placed inside it read
as belonging to:

```c
ui_widget_t* card = ui_panel_create(view);

ui_panel_set_color(card, ui_theme()->surface_sunken);
ui_panel_set_border(card, ui_theme()->border, 1.0);
```

**A separator** — a panel one pixel tall, with no radius to round off its ends:

```c
ui_widget_t* rule = ui_panel_create(view);

ui_panel_set_color(rule, ui_theme()->border);
ui_panel_set_radius(rule, 0.0);

/* in the layout callback */
ui_widget_set_rect(rule, content.x, header.y + header.height + 4, content.width, 1);
```

**A pill** — any radius larger than half the shorter side is clamped to it, so asking for a
large one gives fully round ends at whatever height the layout chooses:

```c
ui_panel_set_radius(badge, 1000.0);
```

Create a panel *before* whatever sits on top of it. Paint order is creation order, so a panel
created after a label covers it.

## Labels

A label is one line of text, transparent behind, clipped to its rectangle and vertically
centred in it. It holds a copy of its text, at most 127 bytes.

**A heading** and **a caption** are the same widget with a different font and colour:

```c
ui_widget_t* title = ui_label_create(view, "Places");
ui_label_set_font(title, UI_FONT_BOLD, ui_theme()->font_size + 4.0);

ui_widget_t* hint = ui_label_create(view, "Double click a row to open it");
ui_label_set_color(hint, ui_theme()->text_muted);
```

**A value against the right edge** — a count, a size, a total — is alignment, not arithmetic:

```c
ui_label_set_align(count, UI_ALIGN_RIGHT);
```

**Padding** keeps text off the edges of a rectangle it shares with a panel, without computing
a second rectangle:

```c
ui_widget_place(status_bar, bar);
ui_widget_place(status_text, bar);
ui_label_set_padding(status_text, 8);
```

**Text that may not fit.** A label clips; it does not ellipsise. For a name or a path whose
length you do not control, cut it to the label's width yourself with
`ui_draw_ellipsize()`, which measures in the same font the label draws with and ends the text
in `…` when it has to shorten it:

```c
#include <aplus/ui-draw.h>

static void fit_label(ui_widget_t* label, const char* text) {

    char fitted[128];

    const ui_rect_t rect = ui_widget_rect(label);

    ui_draw_ellipsize(fitted, sizeof(fitted), text,
                      ui_theme()->font_regular, ui_theme()->font_size, rect.width);

    ui_label_set_text(label, fitted);
}
```

The width only exists once the layout callback has placed the label, so call this from the
end of the layout callback as well as wherever the text changes; `aplus-settings` does exactly
this for every value it shows. Pass the face and size the label really uses — `font_bold` for
a bold label, the size given to `ui_label_set_font()` for a resized one — or the measurement is
of different text. `ui_draw_text_width()` is the measurement on its own, for sizing a
rectangle to its text rather than the other way round.

## Buttons

```c
ui_widget_t* ui_button_create(ui_view_t* view, const char* text, ui_action_fn on_click, void* user);
```

The action runs on release over the button the press began on, so sliding off a button before
letting go cancels it.

**Styles say what a button does, not how loud it is.** One primary button per group, for the
thing a person most likely came to do; danger for what destroys something; default for the
rest:

```c
ui_button_set_style(open, UI_BUTTON_STYLE_PRIMARY);
ui_button_set_style(remove, UI_BUTTON_STYLE_DANGER);
```

**Icons** are names in the icon theme ([icons.md](icons.md)). With text, the icon goes ahead of
it; with empty text, the button is an icon button:

```c
ui_button_set_icon(open, "document-open");

ui_widget_t* up = ui_button_create(view, "", explorer_on_up, NULL);
ui_button_set_icon(up, "go-up");
ui_button_set_icon_size(up, 20);
```

A toolbar of icon buttons wants square rectangles: `aplus-editor` places 28×28 buttons with
20-pixel icons.

**One callback for many buttons.** The `user` argument is per button, so a keypad is one
function and a table:

```c
static const char* digits[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

static void on_digit(ui_widget_t* widget, void* user) {

    (void)widget;

    append((const char*)user);
}

for (int i = 0; i < 10; i++) {
    keys[i] = ui_button_create(view, digits[i], on_digit, (void*)digits[i]);
}
```

`ui_widget_set_user()` writes the same slot, so it changes what the callback receives.

**Enabled follows state.** A button that cannot do anything right now should look it, rather
than accept the press and ignore it. Derive it from the model in one place, every time
something changes:

```c
ui_widget_set_enabled(open, selected != NULL);
ui_widget_set_enabled(remove, selected != NULL);
```

Setters given the value already in place return without repainting, so a refresh function
that rewrites everything is cheap enough to call after every event.

**Keys** reach a button only through your key callback, and the right way to bind one is to
press the button rather than duplicate what it does — `ui_button_activate()` and
`ui_button_set_held()`, covered in
[tutorial-layout.md](tutorial-layout.md#binding-keys-to-buttons).

## Lists

A list is rows of a name, an optional detail against the right edge, an optional icon, and a
pointer of yours. It scrolls by itself and takes the arrow keys once focused.

**Rows carry your data.** The fourth argument comes back from `ui_list_item_user()`, so the
list never has to be searched by its text:

```c
for (size_t i = 0; i < count; i++) {
    ui_list_add_icon(list, files[i].icon, files[i].name, files[i].size_text, &files[i]);
}

file_t* file = ui_list_item_user(list, ui_list_selected(list));
```

`ui_list_item_user()` returns `NULL` for `-1`, so "the selected item, or none" is that one
line. Both strings are copied, so a row can be built from a stack buffer.

**Select and activate are different things.** Selecting a row shows what it is; activating it
— a double click, or `Enter` while the list is focused — does what it is for:

```c
ui_list_on_select(list, on_select, NULL);      /* fill in the details */
ui_list_on_activate(list, on_activate, NULL);  /* open it */
```

**Rebuilding keeps the selection only if you keep it.** `ui_list_clear()` drops every row
*and* the selection, and runs no callback. Remember what was selected by its user pointer,
rebuild, and find it again:

```c
const item_t* keep = ui_list_item_user(list, ui_list_selected(list));

ui_list_clear(list);

int select = -1;

for (size_t i = 0; i < count; i++) {

    const int index = ui_list_add(list, items[i].name, NULL, &items[i]);

    if (&items[i] == keep) {
        select = index;
    }
}

ui_list_select(list, select);
refresh();
```

The explicit `refresh()` at the end is not redundant. When the rebuilt list has nothing to
select, `ui_list_select(list, -1)` finds `-1` already selected — the clear put it there — and
returns without running the select callback, so anything that depends on the selection would
go on describing a row that no longer exists.

Rebuilding from inside the list's own callbacks is fine; destroying the list there is not.
See [Selection and activation](widgets.md#selection-and-activation).

**Row height and icon size** are set together when the defaults do not suit:

```c
ui_list_set_row_height(list, 40);
ui_list_set_icon_size(list, 24);
```

`0` restores either one to what the theme implies, and only then does it follow the theme when
it changes.

## Entries

An entry is a single line of editable text with a placeholder, a caret and no selection.

```c
ui_widget_t* search = ui_entry_create(view, "Search places");

ui_entry_on_change(search, on_change, NULL);  /* after every edit */
ui_entry_on_submit(search, on_submit, NULL);  /* Enter */
```

**Typing needs the focus.** A click on the entry gives it the keyboard; to have the window
start ready to type, give it the focus once the widgets exist:

```c
ui_view_focus(view, search);
```

**Setting the text is silent.** `ui_entry_set_text()` replaces the text and moves the caret to
the end, but does not run the change callback — the caller already knows what it said. If the
rest of the window follows the text, update it in the same breath:

```c
ui_entry_set_text(search, "");
refilter();
```

**An entry can drive a list without giving up the focus.** The entry takes characters,
`Backspace`, `Delete`, `Left`, `Right`, `Home`, `End` and `Enter`, and lets everything else
fall through to the view's key callback — `Up`, `Down`, the page keys and `Escape` among them.
So a search field keeps the keyboard and the window moves the list:

```c
static bool on_key(ui_view_t* view, uint16_t vkey, bool down, void* user) {

    if (!down) {
        return false;
    }

    switch (vkey) {

        case KEY_UP:
            move(-1);
            return true;

        case KEY_DOWN:
            move(1);
            return true;

        default:
            return false;
    }
}
```

`move()` is `ui_list_select()` with the index clamped to the rows there are. The list draws
its selection in the secondary colour while it is not focused, which is the honest picture:
selected, but not where the keys are going. `aplus-launcher` is built entirely on this.

`KEY_*` codes come from `<aplus/input.h>`.

## Showing, hiding and overlapping

`ui_widget_set_visible()` takes a widget out of painting and hit testing, and
`ui_widget_set_enabled()` takes it out of hit testing and dims it. Both release any hover,
press or focus the widget held, and neither gives it back.

Because labels take no pointer input, an **empty state** can sit right over the list it
describes, created after the list so that it is drawn on top, and shown only when there is
nothing to list:

```c
list  = ui_list_create(view);
empty = ui_label_create(view, "No place matches");   /* after the list: drawn over it */

ui_label_set_align(empty, UI_ALIGN_CENTER);

/* in the layout callback: the same rectangle */
ui_widget_place(list, area);
ui_widget_place(empty, area);

/* in refresh() */
ui_widget_set_visible(empty, ui_list_count(list) == 0);
```

The list underneath still gets every click and wheel event, because hit testing only ever
considers interactive widgets.

**Alternating pages** — settings panes, tabs, a wizard — are sets of widgets with one set
visible at a time. `aplus-settings` creates every page's widgets up front and shows one page's
worth; there is no page widget, just a loop of `ui_widget_set_visible()` over each set. Hiding
the page that held the focus drops the focus with it, so give the keyboard back to something
on the new page with `ui_view_focus()`.

## Following the theme

Widgets read the theme while they draw, so a widget left on its defaults follows a theme change
by itself. A colour or a size you *copied* out of the theme with a setter does not, because
the widget now holds a value rather than a role. Gather every such setter into one function,
call it once while building, and hand it to the view to call again on every change:

```c
static void restyle(ui_view_t* view, void* user) {

    const ui_theme_t* theme = ui_theme();

    ui_label_set_font(title, UI_FONT_BOLD, theme->font_size + 4.0);
    ui_label_set_color(hint, theme->text_muted);

    ui_panel_set_color(card, theme->surface_sunken);
    ui_panel_set_border(card, theme->border, 1.0);
}

    restyle(view, NULL);
    ui_view_on_theme(view, restyle, NULL);
```

The view lays out and repaints after calling it, so the layout callback runs against the new
fonts too. See [theming.md](theming.md#following-it-in-an-application).

## Putting it together: ui-places

`ui-places` lists a few well-known places on the system. Typing filters the list by name or
path; the arrows move the selection without leaving the field; the card on the right shows the
selected place, with its path ellipsised to fit; `Open`, `Enter` or a double click hands the
path to `aplus-xopen`; `Remove` takes it off the list; `Escape` clears the search.

```
┌───────────────────────────────────────────────────────────┐
│ Places                                           8 shown  │
│ ┌───────────────────────────────────────────────────────┐ │
│ │ Search places                                         │ │
│ └───────────────────────────────────────────────────────┘ │
│ ┌─────────────────────────────────┐ ┌───────────────────┐ │
│ │ ▣ Applications           folder │ │ Applications      │ │
│ │ ▣ Icons                  folder │ │ /usr/share/appl…  │ │
│ │ ▣ Wallpapers             folder │ │                   │ │
│ │ ▣ Themes                 folder │ │                   │ │
│ │ ▣ Keymaps                folder │ │ ┌──────┐ ┌──────┐ │ │
│ │ ▣ Devices                folder │ │ │Remove│ │ Open │ │ │
│ └─────────────────────────────────┘ └─┴──────┴─┴──────┴─┘ │
└───────────────────────────────────────────────────────────┘
```

Every pattern above is in it:

| Pattern | Where |
|---|---|
| A model of your own, carried by the rows | `places_all[]`, handed to `ui_list_add_icon()` as `user` |
| One refresh function for everything derived | `places_refresh()` — the count, the empty state, the card, which buttons are enabled |
| Rebuilding the list and keeping the selection | `places_fill()` |
| A label fitted to its width | the path, through `ui_draw_ellipsize()` in `places_refresh()`, which the layout callback calls last |
| An entry driving a list | `places_on_key()` takes the arrows and page keys the entry lets through |
| An empty state over the list | `places.empty`, created after the list and placed on the same rectangle |
| Buttons styled by what they do | `Open` primary with an icon, `Remove` danger, both enabled only with a selection |
| Re-applying what was copied out of the theme | `places_restyle()`, called while building and installed with `ui_view_on_theme()` |

### The directory and the Makefile

```bash
mkdir -p apps/extra/ui-places
```

`apps/extra/ui-places/Makefile`:

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

The same as [ui-hello's](getting-started.md#2-the-makefile), and for the same reasons: every
line above the includes is load-bearing. `<aplus/ui-draw.h>` needs nothing more, since its
cairo is already in the link.

### main.c

`apps/extra/ui-places/main.c`:

```c
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <aplus/input.h>
#include <aplus/ui-draw.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>


#define PLACES_MARGIN 16
#define PLACES_GAP    10
#define PLACES_ROW    34
#define PLACES_OPENER "aplus-xopen"


typedef struct {

    const char* name;
    const char* path;
    const char* icon;
    const char* kind;

    bool removed;

} place_t;


static place_t places_all[] = {
    {"Applications", "/usr/share/applications", "applications-system", "folder", false},
    {"Icons", "/usr/share/icons", "folder", "folder", false},
    {"Wallpapers", "/usr/share/images", "preferences-desktop-wallpaper", "folder", false},
    {"Themes", "/usr/share/themes", "preferences-desktop-theme", "folder", false},
    {"Keymaps", "/usr/share/keymaps", "folder", "folder", false},
    {"Devices", "/dev", "drive-harddisk", "folder", false},
    {"System", "/proc", "computer", "folder", false},
    {"Mounts", "/proc/mounts", "text-x-generic", "file", false},
};

#define PLACES_COUNT (sizeof(places_all) / sizeof(places_all[0]))


static struct {

    ui_view_t* view;

    ui_widget_t* heading;
    ui_widget_t* count;
    ui_widget_t* search;
    ui_widget_t* list;
    ui_widget_t* empty;

    ui_widget_t* card;
    ui_widget_t* name;
    ui_widget_t* path;
    ui_widget_t* remove;
    ui_widget_t* open;

} places;


static place_t* places_selected(void) {
    return ui_list_item_user(places.list, ui_list_selected(places.list));
}


/* Everything that follows from the model and the selection, rewritten in full. Setters given
 * the value already in place do nothing, so calling this too often costs nothing.
 */
static void places_refresh(void) {

    const place_t* place = places_selected();

    char text[128];

    snprintf(text, sizeof(text), "%zu shown", ui_list_count(places.list));
    ui_label_set_text(places.count, text);

    /* The empty label sits over the list and is drawn after it; being a label, it takes no
       pointer input, so it can stay there without blocking the list underneath. */
    ui_widget_set_visible(places.empty, ui_list_count(places.list) == 0);

    ui_label_set_text(places.name, place ? place->name : "Nothing selected");

    /* A label clips rather than ellipsising, and a path's tail is the part worth keeping
       out of sight least, so it is fitted here against the width the layout gave it. */
    const ui_rect_t rect = ui_widget_rect(places.path);

    if (place && rect.width > 0) {
        ui_draw_ellipsize(text, sizeof(text), place->path, ui_theme()->font_regular, ui_theme()->font_size, rect.width);
    } else {
        snprintf(text, sizeof(text), "%s", place ? place->path : "");
    }

    ui_label_set_text(places.path, text);

    ui_widget_set_enabled(places.open, place != NULL);
    ui_widget_set_enabled(places.remove, place != NULL);
}


/* Rebuilds the rows from the model and the search text, keeping the selected place selected
 * when it is still listed.
 */
static void places_fill(void) {

    const place_t* keep  = places_selected();
    const char*    query = ui_entry_text(places.search);

    ui_list_clear(places.list);

    int select = -1;

    for (size_t i = 0; i < PLACES_COUNT; i++) {

        place_t* place = &places_all[i];

        if (place->removed) {
            continue;
        }

        if (query[0] && !strcasestr(place->name, query) && !strcasestr(place->path, query)) {
            continue;
        }

        const int index = ui_list_add_icon(places.list, place->icon, place->name, place->kind, place);

        if (place == keep) {
            select = index;
        }
    }

    if (select < 0 && ui_list_count(places.list) > 0) {
        select = 0;
    }

    /* ui_list_clear() drops the selection without telling anyone, and selecting -1 when -1
       is already selected runs no callback, so the refresh is called here rather than
       left to the select callback. */
    ui_list_select(places.list, select);

    places_refresh();
}


static void places_open(void) {

    const place_t* place = places_selected();

    if (!place) {
        return;
    }


    while (waitpid(-1, NULL, WNOHANG) > 0) {
        ;
    }

    const pid_t pid = fork();

    if (pid == 0) {
        execlp(PLACES_OPENER, PLACES_OPENER, place->path, (char*)NULL);
        _exit(127);
    }

    if (pid < 0) {
        fprintf(stderr, "ui-places: cannot open %s: %s\n", place->path, strerror(errno));
    }
}


static void places_on_change(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    places_fill();
}


static void places_on_submit(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    places_open();
}


static void places_on_select(ui_widget_t* widget, int index, void* user) {

    (void)widget;
    (void)index;
    (void)user;

    places_refresh();
}


static void places_on_activate(ui_widget_t* widget, int index, void* user) {

    (void)widget;
    (void)index;
    (void)user;

    places_open();
}


static void places_on_open(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    places_open();
}


static void places_on_remove(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    place_t* place = places_selected();

    if (!place) {
        return;
    }

    place->removed = true;

    places_fill();
}


/* The keys the focused entry lets through: the arrows and the page keys move the list's
 * selection while the keyboard stays in the search field, and Escape empties the field.
 */
static bool places_on_key(ui_view_t* view, uint16_t vkey, bool down, void* user) {

    (void)view;
    (void)user;

    if (!down) {
        return false;
    }


    const int count    = (int)ui_list_count(places.list);
    const int selected = ui_list_selected(places.list);

    int next = selected;

    switch (vkey) {

        case KEY_UP:
            next = selected - 1;
            break;

        case KEY_DOWN:
            next = selected + 1;
            break;

        case KEY_PAGEUP:
            next = selected - 5;
            break;

        case KEY_PAGEDOWN:
            next = selected + 5;
            break;

        case KEY_ESC:

            if (ui_entry_text(places.search)[0]) {

                /* ui_entry_set_text() runs no change callback, so the rows are refilled
                   by hand. */
                ui_entry_set_text(places.search, "");
                places_fill();
            }

            return true;

        default:
            return false;
    }


    if (count == 0) {
        return true;
    }

    if (next < 0) {
        next = 0;
    }

    if (next >= count) {
        next = count - 1;
    }

    ui_list_select(places.list, next);

    return true;
}


static void places_layout(ui_view_t* view, int width, int height, void* user) {

    (void)view;
    (void)user;


    const ui_rect_t content = {

        .x      = PLACES_MARGIN,
        .y      = PLACES_MARGIN,
        .width  = width - 2 * PLACES_MARGIN,
        .height = height - 2 * PLACES_MARGIN,
    };

    if (content.width <= 0 || content.height <= 3 * (PLACES_ROW + PLACES_GAP)) {
        return;
    }


    const ui_rect_t header = {content.x, content.y, content.width, PLACES_ROW};

    ui_grid_t top = ui_grid(header, 2, 1, PLACES_GAP);

    ui_widget_place(places.heading, ui_grid_cell(&top, 0, 0, 1, 1));
    ui_widget_place(places.count, ui_grid_cell(&top, 1, 0, 1, 1));

    ui_widget_set_rect(places.search, content.x, header.y + PLACES_ROW + PLACES_GAP, content.width, PLACES_ROW);


    const int body_y = header.y + 2 * (PLACES_ROW + PLACES_GAP);

    const ui_rect_t body = {content.x, body_y, content.width, content.y + content.height - body_y};

    ui_grid_t columns = ui_grid(body, 5, 1, PLACES_GAP);

    const ui_rect_t list = ui_grid_cell(&columns, 0, 0, 3, 1);
    const ui_rect_t card = ui_grid_cell(&columns, 3, 0, 2, 1);

    ui_widget_place(places.list, list);
    ui_widget_place(places.empty, list);
    ui_widget_place(places.card, card);


    const ui_rect_t inner = ui_rect_inset(card, 12);

    ui_widget_set_rect(places.name, inner.x, inner.y, inner.width, 24);
    ui_widget_set_rect(places.path, inner.x, inner.y + 26, inner.width, 20);

    const ui_rect_t buttons = {inner.x, inner.y + inner.height - PLACES_ROW, inner.width, PLACES_ROW};

    ui_grid_t row = ui_grid(buttons, 2, 1, PLACES_GAP);

    ui_widget_place(places.remove, ui_grid_cell(&row, 0, 0, 1, 1));
    ui_widget_place(places.open, ui_grid_cell(&row, 1, 0, 1, 1));


    /* The path is fitted to its label's width, which has just changed. */
    places_refresh();
}


/* Whatever was copied out of the theme with a setter, applied again: once at startup and
 * after every theme change.
 */
static void places_restyle(ui_view_t* view, void* user) {

    (void)view;
    (void)user;

    const ui_theme_t* theme = ui_theme();

    ui_label_set_font(places.heading, UI_FONT_BOLD, theme->font_size + 4.0);
    ui_label_set_color(places.count, theme->text_muted);
    ui_label_set_color(places.empty, theme->text_muted);
    ui_label_set_color(places.path, theme->text_muted);

    ui_panel_set_color(places.card, theme->surface_sunken);
    ui_panel_set_border(places.card, theme->border, 1.0);
}


static int places_build(ui_view_t* view) {

    places.view = view;

    if ((places.heading = ui_label_create(view, "Places")) == NULL) {
        return -1;
    }

    if ((places.count = ui_label_create(view, "")) == NULL) {
        return -1;
    }

    ui_label_set_align(places.count, UI_ALIGN_RIGHT);


    if ((places.search = ui_entry_create(view, "Search places")) == NULL) {
        return -1;
    }

    ui_entry_on_change(places.search, places_on_change, NULL);
    ui_entry_on_submit(places.search, places_on_submit, NULL);


    if ((places.list = ui_list_create(view)) == NULL) {
        return -1;
    }

    ui_list_on_select(places.list, places_on_select, NULL);
    ui_list_on_activate(places.list, places_on_activate, NULL);

    /* Created after the list, so that it is drawn over it. */
    if ((places.empty = ui_label_create(view, "No place matches")) == NULL) {
        return -1;
    }

    ui_label_set_align(places.empty, UI_ALIGN_CENTER);


    if ((places.card = ui_panel_create(view)) == NULL) {
        return -1;
    }

    if ((places.name = ui_label_create(view, "")) == NULL) {
        return -1;
    }

    ui_label_set_font(places.name, UI_FONT_BOLD, 0.0);

    if ((places.path = ui_label_create(view, "")) == NULL) {
        return -1;
    }


    if ((places.remove = ui_button_create(view, "Remove", places_on_remove, NULL)) == NULL) {
        return -1;
    }

    ui_button_set_style(places.remove, UI_BUTTON_STYLE_DANGER);

    if ((places.open = ui_button_create(view, "Open", places_on_open, NULL)) == NULL) {
        return -1;
    }

    ui_button_set_style(places.open, UI_BUTTON_STYLE_PRIMARY);
    ui_button_set_icon(places.open, "document-open");


    places_restyle(view, NULL);

    return 0;
}


int main(int argc, char** argv) {

    (void)argc;
    (void)argv;


    ui_connection_t* conn = ui_connect(NULL, 5000);

    if (!conn) {
        fprintf(stderr, "ui-places: ui_connect() failed: %s\n", strerror(errno));
        return 1;
    }


    ui_window_t* window = ui_window_create(conn, 560, 340, "Places");

    if (!window) {
        fprintf(stderr, "ui-places: ui_window_create() failed: %s\n", strerror(errno));
        ui_disconnect(conn);
        return 1;
    }


    ui_view_t* view = ui_view_create(window);

    if (!view) {
        fprintf(stderr, "ui-places: ui_view_create() failed: %s\n", strerror(errno));
        ui_window_destroy(window);
        ui_disconnect(conn);
        return 1;
    }


    if (places_build(view) < 0) {
        fprintf(stderr, "ui-places: cannot create the widgets\n");
        ui_view_destroy(view);
        ui_window_destroy(window);
        ui_disconnect(conn);
        return 1;
    }

    places_fill();

    ui_view_on_key(view, places_on_key, NULL);
    ui_view_on_theme(view, places_restyle, NULL);

    /* Installed last, because it runs immediately and everything it places has to exist. */
    ui_view_on_layout(view, places_layout, NULL);

    /* The keyboard starts in the search field, so typing filters straight away. */
    ui_view_focus(view, places.search);


    const int status = ui_view_run(view);

    if (status < 0) {
        fprintf(stderr, "ui-places: ui_view_run() failed: %s\n", strerror(errno));
    }

    ui_view_destroy(view);
    ui_window_destroy(window);
    ui_disconnect(conn);

    return status < 0 ? 1 : 0;
}
```

A few things in it are worth reading twice.

**`places_fill()` ends with `places_refresh()`** even though selecting a row runs the select
callback, which calls it too. The callback does not run when the filtered list is empty —
`-1` was already selected after the clear — and without the explicit call the card would go on
describing a place that is no longer listed, with `Open` still enabled.

**`places_layout()` ends with `places_refresh()`**, because the path is ellipsised against the
label's width, and the width is what the layout just changed. Without it, widening the window
would leave the path cut short at its old width until the selection moved.

**`places_open()` reaps before it forks.** The opener is started and forgotten, so its exit
status is collected the next time something is opened rather than with a `SIGCHLD` handler —
the same arrangement as `aplus-explorer`. The child calls `_exit()` if `execlp()` fails, so
that a missing opener cannot leave a second copy of the application running.

**Nothing is destroyed by hand.** Every widget belongs to the view, and `ui_view_destroy()`
frees them all; the only lifetime to get right is the order of the three teardown calls at the
end of `main()`.

### Build and run

```bash
./makew all
```

```bash
./makew run
```

Open a terminal with `Super+Enter` and start it:

```sh
ui-places
```

Type `share` and five places are left, with `Applications` still selected; `Down` three times
and `Enter` opens the themes directory in `aplus-explorer`. Type something nothing matches and the empty state appears over the list,
with both buttons dimmed. Drag the window narrower and watch the path re-fit to the card.

## Checklist

- Create backgrounds before what sits on them; creation order is paint order.
- Keep every derived piece of state in one refresh function, and call it freely.
- After `ui_list_clear()`, restore the selection yourself and refresh explicitly.
- After `ui_entry_set_text()`, do whatever the change callback would have done.
- Fit text that may not fit with `ui_draw_ellipsize()`, from the layout callback as well.
- Give the keyboard to the widget a person will type into first, with `ui_view_focus()`.
- Gather every value copied out of the theme into one restyle function, and register it with
  `ui_view_on_theme()`.
- Never destroy a widget from inside one of its own callbacks.

## Where to go next

- The full contract of every call used here: [widgets.md](widgets.md).
- Geometry that survives resizing, and binding keys to buttons: [tutorial-layout.md](tutorial-layout.md).
- Drawing what the widgets cannot express: [tutorial-custom-drawing.md](tutorial-custom-drawing.md).
- Icon names and where they come from: [icons.md](icons.md).
