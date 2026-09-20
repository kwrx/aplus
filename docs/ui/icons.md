# Icons

An icon is asked for by name, never by path. `libui` resolves the name against an icon theme
in `/usr/share/icons`, at the size the caller wants, and keeps what it loaded in a cache the
whole process shares. A name nothing answers is not an error: the caller draws no icon and
carries on, which is what lets a listing hand over whatever a `.desktop` file asked for
without checking it first.

```c
#include <aplus/ui-draw.h>

#define UI_ICON_PATH          "/usr/share/icons"
#define UI_ICON_THEME_DEFAULT "aplus"
#define UI_ICON_NAME_MAX      128

const char*      ui_icon_theme(void);
bool             ui_icon_find(const char* name, int size, char* out, size_t max);
cairo_surface_t* ui_icon_load(const char* name, int size);

void ui_draw_icon(cairo_t* cr, ui_rect_t rect, cairo_surface_t* icon);
void ui_draw_icon_named(cairo_t* cr, ui_rect_t rect, const char* name, int size);
```

Most applications never call any of this. A list row takes an icon name and does the rest —
see [Widgets](widgets.md#icons) — and that is how both `aplus-launcher` and `aplus-explorer`
show theirs.

## The theme on disk

```
/usr/share/icons/<theme>/<size>x<size>/<name>.png
```

The theme is `aplus` unless `$UI_ICON_THEME` names another one. The sizes searched are 16, 24,
32 and 48; a second theme is that directory tree and nothing else, since there is no index
file to keep in step.

PNG is the only format. There is no SVG renderer in the sysroot, so a scalable icon would have
nothing to rasterise it.

## The lookup

`ui_icon_load()` takes the size the caller is going to draw at, and picks the file nearest it:
the exact size when the theme has it, then the smallest larger one, then the largest smaller
one. Whatever comes back is scaled to fit the rectangle it is drawn into, so asking for 20 and
being given 24 costs a downscale rather than an empty gutter.

A name carrying a `/` is a path and is taken as it stands, which is what lets a `.desktop` file
point at an icon no theme holds.

Both hits and misses are cached, keyed by name and requested size: a missing icon is looked for
once, not on every frame it is not drawn in. The surface belongs to the cache — it is not to be
destroyed, and it stays valid until enough other icons have been asked for to push it out (32).

## Desktop entries

`Icon` is the key, and it holds a theme name rather than a path in all but the odd case:

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

`aplus-launcher` reads it for the row it lists the application on, and `aplus-explorer` reads it
again for the row a `.desktop` file gets in a directory listing. Neither minds an application
whose icon the theme has never heard of: the fallback is `application-x-executable`.

An application that ships an icon of its own installs it the way it installs its desktop entry,
naming the installed file in `RESOURCES`:

```make
RESOURCES += $(SYSROOT)/usr/share/icons/aplus/48x48/ui-hello.png

$(SYSROOT)/usr/share/icons/aplus/48x48/ui-hello.png: assets/ui-hello.png
	$(QUIET)install -d $(@D)
	$(QUIET)install -m 644 $< $@
```

## What the explorer shows

Everything else in a listing is named by what it is. Directories are `folder`, the `..` row is
`go-up`, and a file is looked up by extension — images, audio, video, archives, fonts and
scripts each have their own — falling back to `application-x-executable` when the file is
executable and `text-x-generic` when it is not.

## The default theme

The `aplus` theme lives in `lib/aplus/ui/assets/icons`, and `lib/aplus/ui/Makefile` installs
every PNG under it. It ships the icons the in-tree applications ask for, the places
`aplus-explorer` lists in its sidebar, and one per file type it knows:

| | |
|---|---|
| Applications | `aplus-calculator`, `aplus-explorer`, `aplus-image-viewer`, `aplus-launcher`, `aplus-terminal`, `gl-gears`, `gl-shaders-scene`, `gl-shaders-triangle` |
| Places and actions | `applications-system`, `drive-harddisk`, `folder`, `go-up`, `system-run`, `user-home` |
| File types | `application-x-executable`, `audio-x-generic`, `font-x-generic`, `image-x-generic`, `package-x-generic`, `text-x-generic`, `text-x-script`, `video-x-generic` |

The PNGs are rendered from the SVG sources in `assets/icons/svg` and committed alongside them,
so an ordinary build needs no SVG renderer. Adding or changing an icon means editing the SVG
and rendering it again:

```sh
./scripts/gen-icons
```

That needs `rsvg-convert` on the *host*, and writes every size of every icon in the theme.
