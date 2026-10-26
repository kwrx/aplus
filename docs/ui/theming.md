# Theming

Reference for the colour scheme in `<aplus/ui-widgets.h>`.

A theme is a struct of *roles*, not of colours. A widget asks for "the accent" or "the text
that goes on a secondary surface" and the theme decides what that is, which is what makes a
second theme a matter of filling in one struct rather than auditing every widget.

## Colours

```c
typedef struct {
    double r;
    double g;
    double b;
    double a;
} ui_color_t;

#define UI_RGB(r, g, b)     {(r) / 255.0, (g) / 255.0, (b) / 255.0, 1.0}
#define UI_RGBA(r, g, b, a) {(r) / 255.0, (g) / 255.0, (b) / 255.0, (a)}

ui_color_t ui_rgb(uint8_t r, uint8_t g, uint8_t b);
ui_color_t ui_rgba(uint8_t r, uint8_t g, uint8_t b, double a);
ui_color_t ui_color_blend(ui_color_t under, ui_color_t over);
```

Components rather than a packed pixel, because everything downstream of them — blending a
hover overlay, dimming a disabled control — is arithmetic, and doing that on 8-bit channels
loses more than it saves. Straight alpha, not premultiplied.

`UI_RGB` and `UI_RGBA` are brace lists rather than compound literals so that they can
initialise a theme at file scope, where a compound literal is not a constant expression. The
cost is that they only work as initialisers:

```c
static const ui_theme_t my_theme = {
    .background = UI_RGB(0xF4, 0xF4, 0xF4),   /* fine */
};

ui_panel_set_color(panel, ui_rgb(0xF4, 0xF4, 0xF4));   /* the function, in an expression */
```

`ui_color_blend(under, over)` composites `over` onto `under` and keeps `under`'s alpha —
washing a translucent overlay over something does not make the thing itself any more opaque.

## The roles

```c
typedef struct {

    ui_color_t background;
    ui_color_t surface;
    ui_color_t surface_sunken;

    ui_color_t primary;
    ui_color_t on_primary;

    ui_color_t secondary;
    ui_color_t on_secondary;

    ui_color_t danger;
    ui_color_t on_danger;

    ui_color_t hover;
    ui_color_t active;

    ui_color_t border;
    ui_color_t focus_ring;

    ui_color_t text;
    ui_color_t text_muted;

    double disabled_fade;

    double corner_radius;
    double font_size;

    const char* font_regular;
    const char* font_medium;
    const char* font_bold;

    const char* font_mono;
    const char* font_mono_bold;
    double font_mono_size;

} ui_theme_t;
```

| Role | Used for |
|---|---|
| `background` | The window backdrop. `ui_view_present()` paints this across every region it repaints, with `CAIRO_OPERATOR_SOURCE` — so on a [translucent window](window-api.md#translucency) its alpha is what shows through, and a theme is where a see-through window is usually decided. |
| `surface` | A panel raised on the backdrop. The default panel colour. |
| `surface_sunken` | A well cut into the backdrop — a list, a display, a text area, anything that should read as recessed. The list's background. |
| `primary` / `on_primary` | The accent, for the one control on a screen that is the point of the screen. `UI_BUTTON_STYLE_PRIMARY`, and the selected row of a focused list in the active window. |
| `secondary` / `on_secondary` | Ordinary controls. `UI_BUTTON_STYLE_DEFAULT`, and the selected row of a list that does not hold the keyboard or whose window is not the active one. |
| `danger` / `on_danger` | Destructive controls. `UI_BUTTON_STYLE_DANGER`. |
| `hover` / `active` | State washes, composited over whatever a control is already painted with, and the wash under the list row the pointer is on. |
| `border` | Panel borders, and the list's and the entry's own; the default border colour. |
| `focus_ring` | What an entry holding the keyboard outlines itself with, two pixels wide over its border. |
| `text` / `text_muted` | The default label colour, and a second one for secondary text — a list's detail column, among others. |

The `on_*` colours travel with their fill. A style resolves to the pair, so it is not
possible for a caller to pick a legible-looking fill and an illegible label to sit on it.

| Scalar | |
|---|---|
| `disabled_fade` | How far a disabled control fades towards the background, 0 to 1. Fading rather than greying keeps a disabled accent recognisable as the accent. |
| `corner_radius` | Default panel radius, and the radius of buttons, lists and entries. |
| `font_size` | Default label and button size, used whenever a `set_font()` gets a size of `0.0` or less. |
| `font_mono_size` | The size [the monospaced faces](#fonts) are drawn at, which decides the cell size. `0.0` leaves it to the application. |

The washes are the part worth understanding before writing a theme. `hover` and `active`
carry their strength in their *alpha*, and are composited over whatever a control already
is. That is why the default scheme spends one colour on hover instead of one per role: the
same white-at-7% lightens the blue accent and the grey button alike, and adding a fourth
button style costs one colour rather than three.

## The default dark scheme

`ui_theme_dark()` returns it. It is what a process holds until it connects, and what the server
sends when no theme file is installed.

| Role | |
|---|---|
| `background` | `#2B2B2B` |
| `surface` | `#343434` |
| `surface_sunken` | `#1C1C1C` |
| `primary` / `on_primary` | `#1B6ACA` / `#FFFFFF` |
| `secondary` / `on_secondary` | `#3A3A3A` / `#EEEEEE` |
| `danger` / `on_danger` | `#C9363C` / `#FFFFFF` |
| `hover` / `active` | white at 7% / white at 14% |
| `border` / `focus_ring` | white at 7% / `#3584E4` |
| `text` / `text_muted` | `#EEEEEE` / `#9A9A9A` |
| `disabled_fade` | `0.55` |
| `corner_radius` / `font_size` | `8.0` / `14.0` |

The greys are chosen to continue the frame `aplus-wm` draws around the window rather than to
stand on their own: `#2B2B2B` content against a `#2B2B2B` titlebar, the same `#EEEEEE` on
top of both. A window whose content started at some other grey would read as a panel bolted
into the frame instead of as the inside of it. `aplus-terminal` takes its background and its
default text colour from `surface_sunken` and `text` for the same reason.

The chrome is deliberately hueless — the window manager marks the active window with a brighter
title and a deeper shadow, not a colour — but an application has one thing on screen that is
the point of the screen and nothing in the chrome to say so. That is what `primary` is for, and
it is the only saturated colour in the scheme. The focus ring is the same blue, brighter: a fill
that carries white text has to be deep enough for the text, at least 4.5:1 and still past it
under the hover wash, while a ring drawn on a dark surface has to be bright enough to be seen,
at least 3:1. `danger` is deepened the same way.

## Fonts

```c
    const char* font_regular;
    const char* font_medium;
    const char* font_bold;

    const char* font_mono;
    const char* font_mono_bold;
    double font_mono_size;
```

Absolute paths. There is no fontconfig in the sysroot, so a family name would have nothing
to resolve against, and cairo's toy font API has nothing to select from. The default theme
names:

```
/usr/share/fonts/ttf/NotoSans-Regular.ttf
/usr/share/fonts/ttf/NotoSans-Medium.ttf
/usr/share/fonts/ttf/NotoSans-Bold.ttf
```

Buttons are drawn in the medium face by default; a theme that leaves `font_medium` `NULL`
gets the regular one there instead. Those are guest paths — inside the running system, not in
the build tree. The sysroot copy is under `root/usr/share/fonts/ttf/`, installed by the
`system-fonts` package, which also holds Inter (regular, medium, bold and their italics), the
Ubuntu family, and the JetBrains Mono, Adwaita Mono and Ubuntu Mono faces if a theme wants
something else. Name static font files: a variable font only ever loads its default instance,
so its medium and bold would draw the same weight as the regular.

`font_mono`, `font_mono_bold` and `font_mono_size` are for text laid out in cells:
`aplus-terminal` draws with both faces, `aplus-editor` with the regular one, and the size
decides how big a cell is. The built-in theme leaves them `NULL` and `0.0`, and so does a
theme file that does not name them; each application then falls back on its own, the way the
window manager falls back on `WM_FONT_PATH` for its titles. The terminal's are the Kconfig
`ATERM_FONT_PATH`, `ATERM_FONT_BOLD_PATH` and `ATERM_FONT_SIZE`, the editor's
`EDITOR_FONT_PATH` and `EDITOR_FONT_SIZE`. A theme that names `font_mono` alone gets it for
bold as well, and a face that cannot be loaded falls back the same way. `dark.conf` and
`light.conf` both name JetBrains Mono at 16:

```
/usr/share/fonts/ttf/JetBrainsMono-Regular.ttf
/usr/share/fonts/ttf/JetBrainsMono-Bold.ttf
```

Three limits come from the face cache in `ui_font.c`:

- **Sixteen distinct font files per process.** Faces are cached by path and never released; a
  theme names up to five, so every switch to a theme with fonts of its own uses up more of them, and
  the seventeenth face draws no text.
- **Paths under 128 bytes.** A longer one is refused rather than truncated, because a
  truncated key would go on matching every other path sharing its prefix and hand back the
  wrong face.
- **A missing font is not fatal.** The file is reported once on stderr and text is simply not
  drawn — a window with blank labels is still a usable thing to debug. If your labels have
  gone missing, check the console before the layout.

## Selecting a theme

```c
const ui_theme_t* ui_theme_dark(void);
const ui_theme_t* ui_theme_system(void);
const char*       ui_theme_name(void);
void              ui_theme_set(const ui_theme_t* theme);
const ui_theme_t* ui_theme(void);
```

`ui_theme()` starts out as the [system theme](#the-system-theme), the one the server sent.
`ui_theme_set()` sets a process-global pointer; `NULL` restores the system theme. A view
snapshots that pointer when it is created, so **the theme has to be set before
`ui_view_create()`**. Changing it afterwards does not restyle a window that already exists.

Two consequences worth stating plainly:

- **The struct must outlive every view created against it.** The view stores the pointer, not
  a copy, and so does every widget. A theme built on the stack of a function that returns is
  a dangling pointer with a window drawn from it. Give it static storage.
- **Per-window themes work,** in a process with more than one window: set the theme, create
  the view, set the next one, create the next view. Each holds what was current when it was
  made.

`ui_theme()` is also how application code reads roles for its own use, which is what keeps
hardcoded colours out of a layout:

```c
    ui_panel_set_color(calc.display, ui_theme()->surface_sunken);
    ui_panel_set_border(calc.display, ui_theme()->border, 1.0);
    ui_label_set_color(calc.expression, ui_theme()->text_muted);
```

Those are copies, though, so they belong in a restyle callback, which the next section covers.

## The system theme

The desktop has one theme, and the server owns it. It reads `/etc/ui/theme.conf`, a link to
one of the files in `/usr/share/themes`. It draws its own frames, taskbar and desktop from
it, and sends the `[ui]` half to every client at connection and again whenever it changes.
Switching is one command:

```
aplus-wmctl list-themes
aplus-wmctl set-theme light
```

The server parses `/usr/share/themes/light.conf`, repoints the link so the next boot starts
with it, repaints and tells every client. A file that does not parse is refused with the line
that is wrong, and nothing changes. `aplus-wmctl reload` reads the theme in use again after an
edit in place. An application does the same through `ui_control_set_theme()`, over the
[control socket](protocol.md#the-control-socket).

### Theme files

```
# Colours are #RRGGBB or #RRGGBBAA, or #RRGGBB followed by an alpha from 0 to 1.

[ui]
background = #F2F2F2
hover      = #000000 0.06
font_size  = 14

[wm]
wallpaper  = /usr/share/images/03.webp
taskbar    = #EBEBEB
```

`[ui]` holds every role of `ui_theme_t`, under the field's own name. `[wm]` is the server's
own palette: `desktop_top`, `desktop_bottom`, `title_active`,
`title_idle`, `outline`, `highlight`, `highlight_idle`, `button`, `button_over`, `button_down`,
`close_over`, `close_down`, `accent`, the `taskbar` colours, the two shadow strengths
`shadow_key` and `shadow_ambient`, the titlebar `font` and `font_size`, and the `wallpaper`.
`taskbar_edge` is the hairline along every edge the taskbar shows the desktop, the rail's and
those of whatever slides out of it, as one line; `taskbar_shadow` is how dark the shadow under
the drawer, the session menu and the popout is, against a window's. The rail casts none.
An empty wallpaper means the gradient between the two desktop colours. There is no frame
colour: the server paints every titlebar and border with `[ui] background`, focused or not, so
a window's frame and its content always meet without a seam. An unfocused window is told apart
by its title, buttons, icon and shadow instead.

Anything a file leaves out keeps the built-in value, so a theme can be a single accent colour.
A key nothing knows is logged and skipped. With no file at all, the server uses the built-in
theme and reports its name as `default`. `dark.conf` spells the built-in values out, and
`light.conf` is its counterpart.

### Following it in an application

```c
static void calc_restyle(ui_view_t* view, void* user) {

    ui_panel_set_color(calc.display, ui_theme()->surface_sunken);
    ui_panel_set_border(calc.display, ui_theme()->border, 1.0);
    ui_label_set_color(calc.expression, ui_theme()->text_muted);
}

    ui_view_on_theme(view, calc_restyle, NULL);
```

`ui_view_dispatch()` handles `UI_EVENT_THEME` by rewriting the system theme in place, calling
the view's `ui_view_on_theme()` callback, refreshing what each widget took from the theme, and
laying out and repainting the whole view. Anything that reads the theme as it draws follows by
itself: the buttons, lists and entries, and panels and labels left on their default colours.
So do the font sizes of labels, buttons and entries, and a list's row height, unless the
application named one. What does not follow is whatever the application copied out of the
theme with a setter, which is what the callback is for; the widgets are refreshed after it, so
a size it sets there is the one that stays.

Two things do not follow:

- **A view created against a theme of the application's own** keeps it, as `ui_theme_set()`
  promises. The launcher derives its theme from `ui_theme_system()`, and its callback derives
  it again.
- **A loop that draws without a view** has to call `ui_theme_update()` itself on
  `UI_EVENT_THEME`, from the thread that draws. `aplus-terminal` does, but only for its faces:
  its colours stay `ui_theme_dark()`'s on purpose. A theme with other monospaced faces or
  another `font_mono_size` keeps the window's size and changes how many cells it holds, and the
  shell is told the new size.

## Writing a theme

For the whole desktop, write a file in `/usr/share/themes` (above). A struct like the one below
is for an application that wants a scheme of its own.

Start from the default and change what you mean to change. Copying the struct wholesale is
deliberate: the fields are documented individually in the header, and an aggregate
initialiser will not compile if a field is added later without being considered.

```c
/* A light scheme. Everything that was a white wash in the dark theme becomes a black one:
 * the washes work by lightening or darkening what is already there, and on a light surface
 * white has nowhere to go. */
static const ui_theme_t app_theme_light = {

    .background     = UI_RGB(0xF2, 0xF2, 0xF2),
    .surface        = UI_RGB(0xFF, 0xFF, 0xFF),
    .surface_sunken = UI_RGB(0xE4, 0xE4, 0xE4),

    .primary    = UI_RGB(0x1E, 0x63, 0xC8),
    .on_primary = UI_RGB(0xFF, 0xFF, 0xFF),

    .secondary    = UI_RGB(0xE0, 0xE0, 0xE0),
    .on_secondary = UI_RGB(0x1C, 0x1C, 0x1C),

    .danger    = UI_RGB(0xC0, 0x39, 0x40),
    .on_danger = UI_RGB(0xFF, 0xFF, 0xFF),

    .hover  = UI_RGBA(0x00, 0x00, 0x00, 0.06),
    .active = UI_RGBA(0x00, 0x00, 0x00, 0.14),

    .border     = UI_RGBA(0x00, 0x00, 0x00, 0.12),
    .focus_ring = UI_RGBA(0x00, 0x00, 0x00, 0.40),

    .text       = UI_RGB(0x1C, 0x1C, 0x1C),
    .text_muted = UI_RGB(0x6B, 0x6B, 0x6B),

    .disabled_fade = 0.55,

    .corner_radius = 6.0,
    .font_size     = 14.0,

    .font_regular = "/usr/share/fonts/ttf/NotoSans-Regular.ttf",
    .font_bold    = "/usr/share/fonts/ttf/NotoSans-Bold.ttf",
};
```

```c
    ui_theme_set(&app_theme_light);

    ui_view_t* view = ui_view_create(window);
```

Two things to check in any new scheme:

**The washes have to run the right way.** They are the one part of the scheme that is not a
colour but a direction. A light theme with the dark theme's white washes gets a hover state
that is invisible on white surfaces and wrong everywhere else.

**Disabled controls have to stay legible.** `disabled_fade` pulls a colour towards
`background`; a fill that is already close to the background disappears entirely at 0.55.
Check a disabled default-style button, which is the closest pair in most schemes.

**A focused list has to look focused.** The selected row moves from `secondary` to `primary`
and the border from `border` to `focus_ring` when a list takes the keyboard. A scheme whose
accent is close to its ordinary control colour loses that distinction entirely, and the arrow
keys start moving a selection with nothing on screen saying why.

A theme does not have to be light or dark to be worth writing. Bumping `font_size` and
`corner_radius` alone, with the default colours, is a legitimate theme — and the only way to
change the button corner radius, which buttons take from the theme and offer no setter for.
