# Animation

This builds `ui-meter`: a level from 0 to 100%, shown as a large number over a bar, with four
buttons that change it. Press one and the bar glides to the new level while the number counts
along with it. Press another before it gets there and it turns round where it is, with no jump.
At 100% the bar starts to pulse, but only while its window is the active one; switch to another
window and the meter goes completely still.

It has the same skeleton as [Getting started](getting-started.md) (a connection, a window, a view,
widgets placed by a layout callback), so this page covers only what is new. The calls themselves
are in the reference, [animation.md](animation.md).

## What moves without any of this

Before writing any animation code, it is worth knowing how much already moves. Buttons ease their
hover and press, lists glide when they scroll and slide their selection from row to row, entries
fade their focus ring in. The window manager grows windows in, fades them out, morphs them on
maximise and flies them into the taskbar. An application that uses `ui_view_run()` gets all of it
and writes nothing; [What the widgets animate](animation.md#what-the-widgets-animate) has the list.

This tutorial is for the next step: something of your own that moves.

## 1. The directory

```bash
mkdir -p apps/extra/ui-meter
```

`apps/extra/ui-meter/Makefile` is the one from
[Getting started](getting-started.md#2-the-makefile), unchanged. `cos()` and `lround()` need no
`-lm`: the C library carries the maths functions itself.

## 2. State, and how it is shown

```c
static int meter_target;

static ui_anim_t meter_level;
```

These are two different things, and keeping them apart is most of what this tutorial teaches.

`meter_target` is the state: the level the user asked for, which the buttons change and which
decides what they are allowed to do next. `meter_level` is presentation, how far the screen has
got towards showing that state. Nothing ever reads the animated value to make a decision. An
application that stores the animated value as its state ends up adding 10 to 37.4% because a
button was pressed mid-glide, and disabling *Full* a quarter of a second after the meter was
already full.

A zero-initialised `ui_anim_t` holds still at 0 and eases out, so neither needs setting up.

## 3. Setting off

```c
static void meter_set(int target) {

    if (target < 0) {
        target = 0;
    }

    if (target > 100) {
        target = 100;
    }

    meter_target = target;

    ui_anim_to(&meter_level, target, UI_MOTION_SLOW, ui_anim_now());

    ui_widget_set_enabled(meter_empty, target > 0);
    ui_widget_set_enabled(meter_less, target > 0);
    ui_widget_set_enabled(meter_more, target < 100);
    ui_widget_set_enabled(meter_full, target < 100);
}


static void meter_on_nudge(ui_widget_t* widget, void* user) {

    (void)widget;

    meter_set(meter_target + (int)(intptr_t)user);
}
```

The state changes at once; the animation only gets told where to go. The buttons are enabled
from the target, not from what is on screen.

`ui_anim_to()` sets off from wherever the value is *right now*. That is the whole of retargeting:
a second press while the bar is still moving starts a new transition from the point the bar had
reached, so it turns round in place rather than jumping back to where the first one started. A
press that asks for the target it already has does nothing at all, which is why nothing here
checks for it.

The duration is `UI_MOTION_SLOW` rather than a number chosen for this program, so the meter moves at
the same pace as a window opening. The four buttons share one callback, each carrying its step
in the user pointer: `-100` and `100` for *Empty* and *Full*, clamped by `meter_set()`.

## 4. Showing a level

```c
static void meter_show(double level) {

    char text[16];

    snprintf(text, sizeof(text), "%ld%%", lround(level));

    ui_label_set_text(meter_value, text);


    ui_rect_t fill = meter_inner;

    fill.width = (int)lround(meter_inner.width * level / 100.0);

    ui_widget_place(meter_fill, fill);
}
```

The bar is two panels: a track, and a fill created after it so that it draws on top. To animate
it, the fill's rectangle is moved. No drawing code is involved. `ui_widget_place()` invalidates
where the widget was and where it now is, so the view repaints just the strip that changed.

This gets called on every frame of a glide, and that is cheap because of how the setters behave.
`ui_label_set_text()` with the string already showing, and `ui_widget_place()` with the rectangle
it already has, return without invalidating anything. A frame in which the number has not ticked
over repaints only the bar.

## 5. Stepping it

Something has to call `meter_show()` once a frame while the level moves, and stop once it has
arrived. That is a tick: given the time, it moves whatever has changed and says when it next
needs to be called.

```c
static bool meter_settle(ui_anim_t* anim, uint64_t now) {

    if (!anim->duration) {
        return false;
    }

    if (ui_anim_running(anim, now)) {
        return true;
    }

    ui_anim_set(anim, ui_anim_target(anim));

    return false;
}


static int meter_tick(uint64_t now) {

    int next = -1;

    if (meter_level.duration) {

        meter_show(ui_anim_value(&meter_level, now));

        if (meter_settle(&meter_level, now)) {
            next = UI_ANIM_FRAME_MS;
        }
    }

    return next;
}
```

For now the tick has only the level to step. Section 7 adds a second animation to it.

`meter_level.duration` is the flag for frames still to show. `ui_anim_to()` sets it, and nothing
clears it until `meter_settle()` calls `ui_anim_set()`. The order inside the `if` matters. The
level is shown first, so the pass in which the transition runs out still shows its final value
(`ui_anim_value()` returns the target exactly once the time is up). Only then is it settled, and
from the next pass on the tick does nothing.

The return value is the deadline: `UI_ANIM_FRAME_MS` (16 ms) while something moves, `-1` when
nothing does. `-1` is what makes an idle meter cost nothing. Never return `0` to mean "as soon as
possible": the loop would spin at full speed and draw the same frame over and over.

## 6. The loop

`ui_view_run()` has no place to call a tick of yours, so the meter runs its own loop. It is
`ui_view_run()` with one extra call:

```c
static int meter_soonest(int a, int b) {

    if (a < 0) {
        return b;
    }

    if (b < 0) {
        return a;
    }

    return a < b ? a : b;
}


static int meter_run(ui_view_t* view, ui_connection_t* conn) {

    while (!ui_view_closed(view)) {

        const int timeout = meter_soonest(meter_tick(ui_anim_now()), ui_view_timeout(view));

        if (ui_view_present(view) < 0) {
            return -1;
        }


        ui_event_t event;

        const int e = ui_next_event(conn, &event, timeout);

        if (e < 0) {
            return -1;
        }

        if (e == 0) {
            continue;
        }

        meter_dispatch(view, &event);


        for (int drained = 0; drained < METER_DRAIN && !ui_view_closed(view); drained++) {

            const int pending = ui_next_event(conn, &event, 0);

            if (pending < 0) {
                return -1;
            }

            if (pending == 0) {
                break;
            }

            meter_dispatch(view, &event);
        }
    }

    return 0;
}
```

Each pass, in order:

1. **Advance.** `meter_tick()` moves the meter's own widgets, and `ui_view_timeout()` steps the
   widgets' own transitions: the buttons' hover and press go on easing only because this loop
   calls it. Each returns a deadline or `-1`, and the sooner one wins.
2. **Present.** Whatever the two invalidated is painted and committed, as one frame.
3. **Wait** for an event, but no longer than the deadline. `0` from `ui_next_event()` means the
   deadline came first, and the loop goes round to draw the next frame. An event means it goes
   round sooner, which is harmless: every value is read off the clock, so an extra pass shows the
   same motion a little more often, never faster.

There is no `usleep()` anywhere, and there should never be one. The timeout handed to
`ui_next_event()` is what paces the frames, and it gives way to an event the moment one arrives.
A sleep would leave the pointer waiting on the animation. With both deadlines at `-1`, the loop
blocks in `ui_next_event()` until something happens, which is the whole cost of an idle window.

The drain is copied from `ui_view_run()`. It takes every event already queued before painting
again, so a burst of pointer motion costs one frame rather than one each.
[widgets.md](widgets.md#driving-your-own-loop) has the reasons behind the rest.

## 7. Something that never stops

A glide has an end. A pulse does not: once the meter is full the bar breathes between its own
colour and a lighter one, for as long as it stays full. A loop like that is not a `ui_anim_t` at
all. Its phase comes straight from the clock, so there is nothing to step:

```c
static void meter_pulse(double strength, uint64_t now) {

    const double wave = 0.5 - 0.5 * cos(2.0 * M_PI * (double)(now % METER_PULSE_MS) / METER_PULSE_MS);

    const ui_color_t base  = ui_theme()->primary;
    const ui_color_t light = ui_color_blend(base, (ui_color_t){1.0, 1.0, 1.0, 0.45});

    ui_panel_set_color(meter_fill, meter_mix(base, light, strength * wave));
}
```

`wave` runs from 0 to 1 and back once every `METER_PULSE_MS`. Starting or stopping it at an
arbitrary point of that cycle would make the colour jump, so the wave is multiplied by a
`strength` that *is* a transition, `meter_glow`, which eases in when the pulse should run and out
when it should not. Because the strength starts from 0, it does not matter where in its cycle
the wave happens to be when the pulse starts; the jump is never visible.

`meter_mix()` interpolates the four components with `ui_lerp()`, because the library has no colour
transition. `ui_color_blend()` makes the lighter shade by washing white over the accent, the same
way the theme's hover wash is applied.

The tick grows a second half:

```c
static int meter_tick(uint64_t now) {

    int next = -1;

    if (meter_level.duration) {

        meter_show(ui_anim_value(&meter_level, now));

        if (meter_settle(&meter_level, now)) {
            next = UI_ANIM_FRAME_MS;
        }
    }


    const bool glowing = meter_active && meter_target == 100 && next < 0;

    ui_anim_to(&meter_glow, glowing ? 1.0 : 0.0, glowing ? UI_MOTION_SLOW : UI_MOTION_NORMAL, now);

    const double glow = ui_anim_value(&meter_glow, now);

    if (glow > 0.0 || meter_glow.duration) {

        meter_pulse(glow, now);

        if (meter_settle(&meter_glow, now) || glow > 0.0) {
            next = UI_ANIM_FRAME_MS;
        }
    }

    return next;
}
```

`ui_anim_to()` is called on every pass with whatever the meter wants right now. That is safe
because asking for the target it already has does nothing, and it saves tracking when the
answer changes. The pulse waits for the level to land (`next < 0`) and for `meter_target`, not
the number on screen, to be 100.

The guard around `meter_pulse()` is not there for tidiness. `ui_panel_set_color()` invalidates
the panel on *every* call, even with the colour it already has (a label's text does not, a
panel's colour does), so calling it unconditionally would repaint the bar on every pass of the
loop, including every pointer move over the window. The guard covers exactly three cases: the
pulse fading in or out (`duration` set), running (`glow > 0`), and the one pass where the fade-out
has just finished. That last pass paints the fill back in plain `primary` and settles the
transition, and the tick goes back to returning `-1`.

`meter_active` is what stops it for a window nobody is using:

```c
static void meter_dispatch(ui_view_t* view, const ui_event_t* event) {

    if (event->type == UI_EVENT_FOCUS) {
        meter_active = event->focus.focused != 0;
    }

    ui_view_dispatch(view, event);
}
```

The server sends `UI_EVENT_FOCUS` when the window is created and every time the focus moves, and
the view still gets the event, since it uses it too. When the window stops being the active one
the pulse fades out, and once it has gone the meter asks for no frames at all. The caret of an
entry stops blinking in an inactive window for the same reason. A loop that runs forever has to
know when to stop, or every window that has one keeps the machine awake sixty times a second.

## 8. Resizing in the middle of a glide

The fill's rectangle depends on the track's, which the layout callback computes, so the layout
has to show the level too:

```c
    meter_inner = ui_rect_inset(track, METER_INSET);

    meter_show(ui_anim_value(&meter_level, ui_anim_now()));
```

`ui_anim_value()` here reads the level at this very moment. A resize that arrives in the middle of a
glide re-lays the bar at the new width at the point the glide had reached, and the next tick
carries on from there. Nothing about the transition itself depends on the window's size, which
is why the value is a level from 0 to 100 and not a width in pixels: a width would be wrong the
moment the window changed size.

## 9. Drawing it yourself instead

A meter built from panels is the easy case. When what moves is drawn by hand into a hole in the
view, as in [Custom drawing](tutorial-custom-drawing.md#cutting-a-hole-in-a-view), two things
change: the tick invalidates the hole instead of moving a widget, and the drawing reads the value
at the clock reading of the same pass.

```c
static int canvas_tick(uint64_t now) {

    if (!canvas_level.duration) {
        return -1;
    }

    canvas_now = now;

    ui_widget_invalidate(canvas_frame);

    return meter_settle(&canvas_level, now) ? UI_ANIM_FRAME_MS : -1;
}
```

`meter_settle()` is the helper from section 5, and `canvas_frame` is the panel the hole sits in. Invalidating it is how the view learns that a frame
is due, since the view does not know about anything drawn by hand: it repaints the panel,
`ui_view_present()` returns `1`, and `canvas_draw()` runs over it as before, reading
`ui_anim_value(&canvas_level, canvas_now)`. The ordering is the same as in the meter. The pass
that settles the transition still paints its last frame, because the invalidation has already
happened by the time the settle runs.

A canvas that zooms or scales a picture while it moves wants
[`CAIRO_FILTER_BILINEAR`](animation.md#cost) on that source.

## 10. The whole thing

`apps/extra/ui-meter/main.c`:

```c
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aplus/ui-anim.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>


#define METER_MARGIN   16
#define METER_GAP      10
#define METER_ROW      40
#define METER_TRACK    14
#define METER_INSET    3
#define METER_PULSE_MS 1200
#define METER_DRAIN    256


static ui_widget_t* meter_card;
static ui_widget_t* meter_value;
static ui_widget_t* meter_track;
static ui_widget_t* meter_fill;
static ui_widget_t* meter_empty;
static ui_widget_t* meter_less;
static ui_widget_t* meter_more;
static ui_widget_t* meter_full;

static ui_rect_t meter_inner;

static int meter_target;
static bool meter_active;

static ui_anim_t meter_level;
static ui_anim_t meter_glow;


/**
 * @brief Mixes two colours a component at a time.
 *
 * @param a The colour at 0.
 * @param b The colour at 1.
 * @param t How far towards b.
 * @return The mix.
 */
static ui_color_t meter_mix(ui_color_t a, ui_color_t b, double t) {

    return (ui_color_t){
        .r = ui_lerp(a.r, b.r, t),
        .g = ui_lerp(a.g, b.g, t),
        .b = ui_lerp(a.b, b.b, t),
        .a = ui_lerp(a.a, b.a, t),
    };
}


/**
 * @brief Shows the meter at a level: the width of the fill and the number above it.
 *
 * @param level The level, 0 to 100.
 */
static void meter_show(double level) {

    char text[16];

    snprintf(text, sizeof(text), "%ld%%", lround(level));

    ui_label_set_text(meter_value, text);


    ui_rect_t fill = meter_inner;

    fill.width = (int)lround(meter_inner.width * level / 100.0);

    ui_widget_place(meter_fill, fill);
}


/**
 * @brief Colours the fill for one moment of the pulse, washed lighter the stronger the glow.
 *
 * @param strength How strong the glow is, 0 to 1.
 * @param now The clock reading the pulse is drawn at.
 */
static void meter_pulse(double strength, uint64_t now) {

    const double wave = 0.5 - 0.5 * cos(2.0 * M_PI * (double)(now % METER_PULSE_MS) / METER_PULSE_MS);

    const ui_color_t base  = ui_theme()->primary;
    const ui_color_t light = ui_color_blend(base, (ui_color_t){1.0, 1.0, 1.0, 0.45});

    ui_panel_set_color(meter_fill, meter_mix(base, light, strength * wave));
}


/**
 * @brief Settles a transition on its target once it has run out.
 *
 * @param anim The transition.
 * @param now The clock reading of the frame about to be painted.
 * @return true while it still has frames to paint.
 */
static bool meter_settle(ui_anim_t* anim, uint64_t now) {

    if (!anim->duration) {
        return false;
    }

    if (ui_anim_running(anim, now)) {
        return true;
    }

    ui_anim_set(anim, ui_anim_target(anim));

    return false;
}


/**
 * @brief Advances the meter's own animations to a moment, moving whatever they changed.
 *
 * @param now The clock reading of the frame about to be painted.
 * @return How long until the next frame is due, or -1 when nothing is moving.
 */
static int meter_tick(uint64_t now) {

    int next = -1;

    if (meter_level.duration) {

        meter_show(ui_anim_value(&meter_level, now));

        if (meter_settle(&meter_level, now)) {
            next = UI_ANIM_FRAME_MS;
        }
    }


    const bool glowing = meter_active && meter_target == 100 && next < 0;

    ui_anim_to(&meter_glow, glowing ? 1.0 : 0.0, glowing ? UI_MOTION_SLOW : UI_MOTION_NORMAL, now);

    const double glow = ui_anim_value(&meter_glow, now);

    if (glow > 0.0 || meter_glow.duration) {

        meter_pulse(glow, now);

        if (meter_settle(&meter_glow, now) || glow > 0.0) {
            next = UI_ANIM_FRAME_MS;
        }
    }

    return next;
}


/**
 * @brief Picks the sooner of two deadlines, either of which may be -1 for none.
 *
 * @param a A delay in milliseconds, or -1.
 * @param b Another, or -1.
 * @return The shorter of the two, or -1 when neither is set.
 */
static int meter_soonest(int a, int b) {

    if (a < 0) {
        return b;
    }

    if (b < 0) {
        return a;
    }

    return a < b ? a : b;
}


/**
 * @brief Moves the meter to a new level, gliding there from wherever it is now.
 *
 * @param target The level to end at, clamped to 0..100.
 */
static void meter_set(int target) {

    if (target < 0) {
        target = 0;
    }

    if (target > 100) {
        target = 100;
    }

    meter_target = target;

    ui_anim_to(&meter_level, target, UI_MOTION_SLOW, ui_anim_now());

    ui_widget_set_enabled(meter_empty, target > 0);
    ui_widget_set_enabled(meter_less, target > 0);
    ui_widget_set_enabled(meter_more, target < 100);
    ui_widget_set_enabled(meter_full, target < 100);
}


/**
 * @brief Moves the meter by the step its button carries.
 *
 * @param widget The button.
 * @param user The step, as an intptr_t.
 */
static void meter_on_nudge(ui_widget_t* widget, void* user) {

    (void)widget;

    meter_set(meter_target + (int)(intptr_t)user);
}


/**
 * @brief Places everything, at startup and after every resize, the fill where the level is right now.
 *
 * @param view The view.
 * @param width The content width.
 * @param height The content height.
 * @param user Unused.
 */
static void meter_layout(ui_view_t* view, int width, int height, void* user) {

    (void)view;
    (void)user;


    ui_rect_t content = {

        .x      = METER_MARGIN,
        .y      = METER_MARGIN,
        .width  = width - 2 * METER_MARGIN,
        .height = height - 2 * METER_MARGIN,
    };

    if (content.width <= 2 * METER_MARGIN || content.height <= METER_ROW + 2 * METER_GAP + 2 * METER_MARGIN + METER_TRACK) {
        return;
    }


    ui_rect_t card = {

        .x      = content.x,
        .y      = content.y,
        .width  = content.width,
        .height = content.height - METER_ROW - METER_GAP,
    };

    ui_widget_place(meter_card, card);


    const ui_rect_t inside = ui_rect_inset(card, METER_MARGIN);

    ui_rect_t value = {

        .x      = inside.x,
        .y      = inside.y,
        .width  = inside.width,
        .height = inside.height - METER_TRACK - METER_GAP,
    };

    ui_widget_place(meter_value, value);
    ui_label_set_font(meter_value, UI_FONT_BOLD, value.height * 0.6);


    ui_rect_t track = {

        .x      = inside.x,
        .y      = inside.y + inside.height - METER_TRACK,
        .width  = inside.width,
        .height = METER_TRACK,
    };

    ui_widget_place(meter_track, track);

    meter_inner = ui_rect_inset(track, METER_INSET);

    meter_show(ui_anim_value(&meter_level, ui_anim_now()));


    ui_rect_t buttons = {

        .x      = content.x,
        .y      = content.y + content.height - METER_ROW,
        .width  = content.width,
        .height = METER_ROW,
    };

    ui_grid_t grid = ui_grid(buttons, 4, 1, METER_GAP);

    ui_widget_place(meter_empty, ui_grid_cell(&grid, 0, 0, 1, 1));
    ui_widget_place(meter_less, ui_grid_cell(&grid, 1, 0, 1, 1));
    ui_widget_place(meter_more, ui_grid_cell(&grid, 2, 0, 1, 1));
    ui_widget_place(meter_full, ui_grid_cell(&grid, 3, 0, 1, 1));
}


/**
 * @brief Creates the widgets, in paint order: the card, the number, the track, and the fill over it.
 *
 * @param view The view to create them in.
 * @return 0 on success, -1 when one cannot be created.
 */
static int meter_build(ui_view_t* view) {

    const ui_theme_t* theme = ui_theme();

    if ((meter_card = ui_panel_create(view)) == NULL) {
        return -1;
    }

    ui_panel_set_color(meter_card, theme->surface_sunken);
    ui_panel_set_border(meter_card, theme->border, 1.0);


    if ((meter_value = ui_label_create(view, "0%")) == NULL) {
        return -1;
    }

    ui_label_set_align(meter_value, UI_ALIGN_CENTER);


    if ((meter_track = ui_panel_create(view)) == NULL) {
        return -1;
    }

    ui_panel_set_color(meter_track, theme->surface);
    ui_panel_set_radius(meter_track, METER_TRACK / 2.0);


    if ((meter_fill = ui_panel_create(view)) == NULL) {
        return -1;
    }

    ui_panel_set_color(meter_fill, theme->primary);
    ui_panel_set_radius(meter_fill, (METER_TRACK - 2 * METER_INSET) / 2.0);


    if ((meter_empty = ui_button_create(view, "Empty", meter_on_nudge, (void*)(intptr_t)-100)) == NULL) {
        return -1;
    }

    if ((meter_less = ui_button_create(view, "-10", meter_on_nudge, (void*)(intptr_t)-10)) == NULL) {
        return -1;
    }

    if ((meter_more = ui_button_create(view, "+10", meter_on_nudge, (void*)(intptr_t)10)) == NULL) {
        return -1;
    }

    if ((meter_full = ui_button_create(view, "Full", meter_on_nudge, (void*)(intptr_t)100)) == NULL) {
        return -1;
    }

    ui_button_set_style(meter_more, UI_BUTTON_STYLE_PRIMARY);

    return 0;
}


/**
 * @brief Feeds one event to the view, keeping note of whether the window is the active one.
 *
 * @param view The view.
 * @param event The event.
 */
static void meter_dispatch(ui_view_t* view, const ui_event_t* event) {

    if (event->type == UI_EVENT_FOCUS) {
        meter_active = event->focus.focused != 0;
    }

    ui_view_dispatch(view, event);
}


/**
 * @brief Runs the view with the meter's animations stepped alongside the widgets' own.
 *
 * @param view The view to run.
 * @param conn The connection its window belongs to.
 * @return 0 on a clean close, -1 otherwise.
 */
static int meter_run(ui_view_t* view, ui_connection_t* conn) {

    while (!ui_view_closed(view)) {

        const int timeout = meter_soonest(meter_tick(ui_anim_now()), ui_view_timeout(view));

        if (ui_view_present(view) < 0) {
            return -1;
        }


        ui_event_t event;

        const int e = ui_next_event(conn, &event, timeout);

        if (e < 0) {
            return -1;
        }

        if (e == 0) {
            continue;
        }

        meter_dispatch(view, &event);


        for (int drained = 0; drained < METER_DRAIN && !ui_view_closed(view); drained++) {

            const int pending = ui_next_event(conn, &event, 0);

            if (pending < 0) {
                return -1;
            }

            if (pending == 0) {
                break;
            }

            meter_dispatch(view, &event);
        }
    }

    return 0;
}


int main(int argc, char** argv) {

    (void)argc;
    (void)argv;


    ui_connection_t* conn = ui_connect(NULL, 5000);

    if (!conn) {
        fprintf(stderr, "ui-meter: ui_connect() failed: %s\n", strerror(errno));
        return 1;
    }


    ui_window_t* window = ui_window_create(conn, 360, 220, "ui-meter");

    if (!window) {
        fprintf(stderr, "ui-meter: ui_window_create() failed: %s\n", strerror(errno));
        ui_disconnect(conn);
        return 1;
    }


    ui_view_t* view = ui_view_create(window);

    if (!view) {
        fprintf(stderr, "ui-meter: ui_view_create() failed: %s\n", strerror(errno));
        ui_window_destroy(window);
        ui_disconnect(conn);
        return 1;
    }


    if (meter_build(view) < 0) {
        fprintf(stderr, "ui-meter: cannot create the widgets\n");
        ui_view_destroy(view);
        ui_window_destroy(window);
        ui_disconnect(conn);
        return 1;
    }

    ui_view_on_layout(view, meter_layout, NULL);

    meter_set(40);


    const int status = meter_run(view, conn);

    if (status < 0) {
        fprintf(stderr, "ui-meter: the event loop failed: %s\n", strerror(errno));
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

Open a terminal with `Super+Enter` and start it:

```sh
ui-meter
```

The window grows in, and the bar glides from empty to 40% while it does. Then try:

- **`+10` several times, quickly.** Each press turns the bar towards the new level from wherever
  it is. It never jumps, never queues the presses up, and never runs past the last one.
- **`Empty`, then `Full` before the bar has emptied.** It turns round in place. The second
  transition starts at full speed, which is why the reversal looks immediate.
- **`Full`.** The bar glides to the end, and only once it has landed does the pulse fade in.
- **`Super+Enter` while it pulses.** The new terminal takes the focus, the pulse fades out, and
  the meter stops waking up at all until it gets another event. Click its taskbar entry and the
  pulse fades back in.
- **Drag an edge during a glide.** The bar keeps gliding at its new width.

## Checklist

- The state is the target; the animated value is only for drawing. Decide nothing from it.
- Read the clock once per pass, and step and paint everything with that one reading.
- A tick returns `UI_ANIM_FRAME_MS` while something moves and `-1` when nothing does. Never `0`,
  and never a sleep in the loop.
- Settle a transition after showing its last frame, not before.
- Hand `ui_next_event()` the sooner of your deadline and `ui_view_timeout()`'s, or the widgets
  freeze half way through their own transitions.
- Setters that invalidate on every call (a panel's colour, radius and border, a label's font)
  belong behind a check that the value is actually moving.
- Take durations from `UI_MOTION_FAST`, `UI_MOTION_NORMAL` and `UI_MOTION_SLOW`.
- Stop anything that loops forever once the window is not the active one.

## Where to go next

- Every call, the curves, the shared durations, and what the server animates on its own:
  [animation.md](animation.md).
- Writing a widget that animates itself through the `tick` hook:
  [widgets.md](widgets.md#adding-a-widget).
- Drawing what the widgets cannot: [tutorial-custom-drawing.md](tutorial-custom-drawing.md).
