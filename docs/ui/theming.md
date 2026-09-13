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
    const char* font_bold;

} ui_theme_t;
```

| Role | Used for |
|---|---|
| `background` | The window backdrop. `ui_view_present()` paints this across every region it repaints. |
| `surface` | A panel raised on the backdrop. The default panel colour. |
| `surface_sunken` | A well cut into the backdrop — a display, a text area, anything that should read as recessed. |
| `primary` / `on_primary` | The accent, for the one control on a screen that is the point of the screen. `UI_BUTTON_STYLE_PRIMARY`. |
| `secondary` / `on_secondary` | Ordinary controls. `UI_BUTTON_STYLE_DEFAULT`. |
| `danger` / `on_danger` | Destructive controls. `UI_BUTTON_STYLE_DANGER`. |
| `hover` / `active` | State washes, composited over whatever a control is already painted with. |
| `border` | Panel borders; the default border colour. |
| `focus_ring` | Declared for a focus indicator the widget set does not draw yet. |
| `text` / `text_muted` | The default label colour, and a second one for secondary text. |

The `on_*` colours travel with their fill. A style resolves to the pair, so it is not
possible for a caller to pick a legible-looking fill and an illegible label to sit on it.

| Scalar | |
|---|---|
| `disabled_fade` | How far a disabled control fades towards the background, 0 to 1. Fading rather than greying keeps a disabled accent recognisable as the accent. |
| `corner_radius` | Default panel radius and the button radius, which buttons do not override. |
| `font_size` | Default label and button size, used whenever a `set_font()` gets a size of `0.0` or less. |

The washes are the part worth understanding before writing a theme. `hover` and `active`
carry their strength in their *alpha*, and are composited over whatever a control already
is. That is why the default scheme spends one colour on hover instead of one per role: the
same white-at-9% lightens the blue accent and the grey button alike, and adding a fourth
button style costs one colour rather than three.

## The default dark scheme

`ui_theme_dark()` returns it, and it is what a process starts with.

| Role | |
|---|---|
| `background` | `#2B2B2B` |
| `surface` | `#353535` |
| `surface_sunken` | `#161616` |
| `primary` / `on_primary` | `#3D7EE0` / `#FFFFFF` |
| `secondary` / `on_secondary` | `#3C3C3C` / `#E8E8E8` |
| `danger` / `on_danger` | `#D6454D` / `#FFFFFF` |
| `hover` / `active` | white at 9% / white at 20% |
| `border` / `focus_ring` | white at 8% / white at 45% |
| `text` / `text_muted` | `#E8E8E8` / `#7F7F7F` |
| `disabled_fade` | `0.55` |
| `corner_radius` / `font_size` | `6.0` / `14.0` |

The greys are chosen to continue the frame `aplus-wm` draws around the window rather than to
stand on their own: `#2B2B2B` content against a `#2B2B2B` titlebar, the same `#E8E8E8` on
top of both. A window whose content started at some other grey would read as a panel bolted
into the frame instead of as the inside of it.

The chrome is deliberately hueless — the window manager marks the active window with a ring,
not a colour — but an application has one thing on screen that is the point of the screen and
no ring to say so. That is what `primary` is for, and it is the only saturated colour in the
scheme.

## Fonts

```c
    const char* font_regular;
    const char* font_bold;
```

Absolute paths. There is no fontconfig in the sysroot, so a family name would have nothing
to resolve against, and cairo's toy font API has nothing to select from. The default theme
names:

```
/usr/share/fonts/ttf/Ubuntu-R.ttf
/usr/share/fonts/ttf/Ubuntu-B.ttf
```

Those are guest paths — inside the running system, not in the build tree. The sysroot copy is
under `root/usr/share/fonts/ttf/`, which also holds the Ubuntu light, medium, condensed and
mono faces if a theme wants something else.

Three limits come from the face cache in `ui_font.c`:

- **Four distinct font files per process.** Faces are cached by path and never released; a
  theme names two, so the budget is a theme plus one more.
- **Paths under 128 bytes.** A longer one is refused rather than truncated, because a
  truncated key would go on matching every other path sharing its prefix and hand back the
  wrong face.
- **A missing font is not fatal.** The file is reported once on stderr and text is simply not
  drawn — a window with blank labels is still a usable thing to debug. If your labels have
  gone missing, check the console before the layout.

## Selecting a theme

```c
const ui_theme_t* ui_theme_dark(void);
void              ui_theme_set(const ui_theme_t* theme);
const ui_theme_t* ui_theme(void);
```

`ui_theme_set()` sets a process-global pointer; `NULL` restores the dark default. A view
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

## Writing a theme

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

    .font_regular = "/usr/share/fonts/ttf/Ubuntu-R.ttf",
    .font_bold    = "/usr/share/fonts/ttf/Ubuntu-B.ttf",
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

A theme does not have to be light or dark to be worth writing. Bumping `font_size` and
`corner_radius` alone, with the default colours, is a legitimate theme — and the only way to
change the button corner radius, which buttons take from the theme and offer no setter for.
