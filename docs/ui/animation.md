# Animation

Everything that moves on the desktop is the same thing underneath: a button lighting up, a list
gliding to a row, a window growing into place. In each case one number travels from one value to
another over a few hundred milliseconds along a curve, and whoever paints reads it off the clock.
`<aplus/ui-anim.h>` is that number. The widgets use it, `aplus-wm` uses it, and an application can
use it for whatever it draws itself, so everything on the screen moves at one pace.

There is no animation thread and no timer. A transition is a value plus the moment it set off.
Whoever paints asks where it has got to by now, and whoever runs the loop asks how long it may
sleep before the next frame is due. With nothing moving, the loop blocks on the socket and the
process costs nothing at all.

The walkthrough is [tutorial-animation.md](tutorial-animation.md); this page is the reference.

## The header

```c
#include <aplus/ui-anim.h>
```

The header names no cairo type and no connection. It is part of `libui.a` (`ui_anim.c`), so any
client linking `-lui` has it, a surface-layer client with no view included.

## The clock

```c
uint64_t ui_anim_now(void);
```

Milliseconds of `CLOCK_MONOTONIC`, from an arbitrary origin, or `0` when there is no clock. Every
other call takes the time as an argument rather than reading the clock itself, so one reading
taken at the top of a frame times everything painted in it.

## A transition

```c
typedef enum {
    UI_EASE_OUT = 0,
    UI_EASE_IN,
    UI_EASE_IN_OUT,
    UI_EASE_LINEAR,
} ui_ease_t;

typedef struct {
    double from;
    double to;
    uint64_t start;
    uint32_t duration;
    ui_ease_t ease;
} ui_anim_t;
```

A zero-initialised `ui_anim_t` holds still at `0` and eases out, so a static or `calloc()`ed one
needs no setting up. `ease` is the one field you write by hand: nothing below ever changes it, so set
it once and it applies to every transition the struct goes through.

```c
void   ui_anim_set(ui_anim_t* anim, double value);
void   ui_anim_to(ui_anim_t* anim, double target, uint32_t duration, uint64_t now);
double ui_anim_value(const ui_anim_t* anim, uint64_t now);
double ui_anim_target(const ui_anim_t* anim);
bool   ui_anim_running(const ui_anim_t* anim, uint64_t now);
```

| Call | |
|---|---|
| `ui_anim_set()` | Holds the value still, cutting short whatever it was doing. `duration` becomes `0` |
| `ui_anim_to()` | Sets off towards `target` from wherever the value is at `now`. A `duration` of `0` is `ui_anim_set()` |
| `ui_anim_value()` | The eased value at `now`: `from` before the start, `to` once the duration has run out |
| `ui_anim_target()` | Where it is heading, which is where it is when nothing is moving |
| `ui_anim_running()` | `true` until the duration has run out |

### Changing course

`ui_anim_to()` starts from `ui_anim_value(anim, now)`: wherever the value has got to, in the middle
of a transition or not. A hover that ends half way through fading in fades out from half way, and
a second click that lands before the first glide has finished turns it round where it stands.

The position never jumps, but the speed does. The new transition starts its curve from the
beginning, and with `UI_EASE_OUT` that means it starts at full speed: a reversal happens at once,
which reads as the interface answering rather than finishing what it was doing first. Every
change of course runs for the full duration, however little distance is left.

Asking for the target the transition already has does nothing. It does not restart it, and it does
not change its duration. Calling `ui_anim_to()` on every frame with the state you want is
therefore safe, and often the simplest way to write it. The other side of that: to replay a
transition to the value it already holds (a flash, a shake), `ui_anim_set()` it back to its
starting point first.

Use a duration of `0`, or `ui_anim_set()`, for what must not animate: the state a window opens
with, a value that follows the pointer exactly, a reset.

### Running and settled

`ui_anim_running()` turns false the moment the duration runs out, but the struct still remembers
that it moved. `duration` stays non-zero until something calls `ui_anim_set()`. Both the server
and the list use that as the flag for frames still to paint. They step a transition every frame, and
once it is no longer running they paint it one last time, at its target, and settle it:

```c
/**
 * @brief Settles a transition on its target once it has run out.
 *
 * @param anim The transition.
 * @param now The clock reading of the frame about to be painted.
 * @return true while it still has frames to paint.
 */
static bool app_settle(ui_anim_t* anim, uint64_t now) {

    if (!anim->duration) {
        return false;
    }

    if (ui_anim_running(anim, now)) {
        return true;
    }

    ui_anim_set(anim, ui_anim_target(anim));

    return false;
}
```

After that, `anim->duration == 0` means there is nothing left to paint, and the loop can stop
asking for frames.

## Curves

```c
double ui_ease(ui_ease_t ease, double t);
double ui_lerp(double a, double b, double t);
```

| Curve | Shape | For |
|---|---|---|
| `UI_EASE_OUT` | `1 − (1 − t)³`: quick off the mark, settles gently | Anything answering input, and anything arriving. The default, and what nearly everything in the tree uses |
| `UI_EASE_IN` | `t³`: slow to start, leaves fast | Something leaving for good. Nothing in the tree uses it today |
| `UI_EASE_IN_OUT` | Cubic, symmetric: slow at both ends | Something travelling between two places that both matter: the maximise morph, a window flying into the taskbar |
| `UI_EASE_LINEAR` | `t` | Progress that has to be honest, and loops, where easing would make every lap lurch |

`ui_ease()` clamps `t` to `0..1`, and works on its own for a progress you compute yourself:
`ui_ease(UI_EASE_IN_OUT, elapsed / period)`. `ui_lerp()` does not clamp, and extrapolates past
either end.

There is no colour transition. Interpolate the four components with `ui_lerp()`, or wash one
colour over another with `ui_color_blend()` at an alpha that eases. The second is how a button's
hover is drawn: `theme->hover` blended over the fill at the strength the transition has reached.

## The motion scale

```c
#define UI_ANIM_FRAME_MS 16

#define UI_MOTION_FAST   110
#define UI_MOTION_NORMAL 180
#define UI_MOTION_SLOW   240
```

The toolkit and the server share three durations rather than choosing one per transition, and an
application that picks from the same three moves at the desktop's pace:

| Duration | ms | Used for |
|---|---|---|
| `UI_MOTION_FAST` | 110 | What answers the pointer: hover in and out, a list's selection sliding between rows, an entry's focus ring, the taskbar's hover |
| `UI_MOTION_NORMAL` | 180 | State settling: a press letting go, a list row the pointer left fading out, scroll glides, focus moving between windows, a closed window fading out |
| `UI_MOTION_SLOW` | 240 | Something appearing or travelling: a window opening, the maximise morph, minimise and restore, the launcher drawer sliding out |

None of them goes past a quarter of a second, and that limit is deliberate. Motion here shows
what happened; it is not an effect to sit and watch, and a transition still running when the hand
has already moved on to the next thing is in the way.

`UI_ANIM_FRAME_MS` is how soon a running transition asks to be painted again, about sixty frames
a second. It is a request, not a promise: a frame that takes longer to paint delays the next one.
Because every value is read off the clock rather than stepped once per frame, a slow frame makes
the motion skip ahead, never drag. A transition always ends on time.

## Frames

A pass of the loop is: advance whatever moves, paint, then wait for an event *or* the next frame,
whichever comes first.

```c
int ui_view_timeout(ui_view_t* view);
```

`ui_view_timeout()` is the advance for a view. It calls the `tick` of every visible widget with
one clock reading, and each widget invalidates whatever it moved and says when it next needs a
frame. The view returns the soonest of those, or `-1` when nothing is moving, and that number is
what the loop hands to `ui_next_event()` as its timeout. `ui_view_run()` does all of this for
itself. A loop of your own has to call it once a pass, before it blocks, or every hover and
glide in the view freezes half way. See
[widgets.md](widgets.md#driving-your-own-loop).

Not everything that runs on a clock is a transition. A focused entry's caret asks for its next
frame when the next blink is due, 530 ms away, not in 16. `-1` is what keeps an idle window idle:
no timer is ever armed that nothing needs.

One clock reading per frame. `ui_view_present()` notes the time as it starts to paint, and every
widget draws its transitions at that time, so two transitions that set off together are always at
the same point of their curves in the same frame. `aplus-wm` does the same with the frame it is
about to composite. A widget whose state changes twice before a frame goes out simply eases from
wherever it had got to.

The state itself never waits for the animation. `ui_list_selected()` returns the new row the
moment it changes, `ui_widget_rect()` the new rectangle, and hit testing uses where things *are*,
not where they are drawn: a click in the middle of a scroll glide lands on the row under the
pointer. Only the drawing catches up.

## What the widgets animate

| Widget | What moves | How |
|---|---|---|
| Button | The hover wash | In and out over `UI_MOTION_FAST` |
| Button | The press wash, and a 4% shrink towards the centre | In over 60 ms, out over `UI_MOTION_NORMAL`. The same for a button held down with `ui_button_set_held()` |
| List | The row under the pointer | The new row fades in over `UI_MOTION_FAST`, the row it left fades out over `UI_MOTION_NORMAL`, both at once |
| List | The selection | Slides from the old row to the new one over `UI_MOTION_FAST`. A selection made from none appears in place |
| List | Scrolling | The wheel, a click on the scrollbar track, and keeping the selection in view glide over `UI_MOTION_NORMAL`. Dragging the thumb follows the pointer exactly |
| Entry | The focus ring | In and out over `UI_MOTION_FAST` |
| Entry | The caret | Blinks every 530 ms while the entry is focused, enabled and in the active window. No easing |

Panels and labels do not animate, and there is no switch to make the others stop.

## What the server animates

| What | How | Duration, curve |
|---|---|---|
| A window opening | Grows from 94% of its size and fades in, starting at its first commit | `UI_MOTION_SLOW`, out |
| A window closing | A picture of its last frame shrinks to 94% and fades out, from however far it had faded in | `UI_MOTION_NORMAL`, out |
| The focus moving | Frame and titlebar colours crossfade on both windows | `UI_MOTION_NORMAL` |
| Maximise and restore | The old picture stretches towards the new frame and fades, while the window at its new size grows out of it | `UI_MOTION_SLOW`, in-out |
| Minimise and restore | The window shrinks into its taskbar entry as it fades, and comes back out the same way | `UI_MOTION_SLOW`, in-out |
| Titlebar buttons | Hover | `UI_MOTION_FAST` |
| The taskbar | Hover on an entry or on the launcher's button | `UI_MOTION_FAST` |
| The taskbar's label | Pops out of the rail and glides from item to item over `UI_MOTION_NORMAL`, and goes away over `UI_MOTION_FAST` | |
| The launcher drawer | Slides out over `UI_MOTION_SLOW` and back in over `UI_MOTION_NORMAL`, and follows the launcher's height as its results change over `UI_MOTION_NORMAL` | |

None of this goes over the wire. The protocol has no message for it, and a client has nothing to opt
into and nothing to turn off, but a few of these depend on what the client does:

- **The open animation starts at the first commit.** Whatever that commit holds is what grows in,
  so paint the whole first frame before committing it. `ui_view_run()` does, since its first
  present paints every widget.
- **The close animation needs nothing from the client.** The server has kept a picture of the last
  frame, so exit straight away; there is nothing to wait for.
- **A maximise sends its configure as the morph starts.** The morph fades into whatever the client
  commits at the new size, so a client that answers the configure promptly morphs into its real
  layout.
- **A drawer window** (`UI_WINDOW_DRAWER`) slides out of the rail instead of growing in, and does not
  fade when it closes.

## Cost

Every frame of an animation is a repaint of whatever moved, a commit, and a composite in the
server. Invalidate only what changed: the widgets each invalidate their own rectangle, and the
list only the strips of the rows involved. A `ui_view_invalidate()` on every frame repaints the
whole window sixty times a second to move one thing.

Mind what the setters do while something moves. `ui_label_set_text()`, `ui_widget_place()` and
`ui_widget_set_enabled()` return without invalidating anything when nothing changed, so calling
them on every frame costs nothing extra. `ui_panel_set_color()`, `ui_panel_set_radius()`,
`ui_panel_set_border()` and `ui_label_set_font()` invalidate on every call: call those only while
the value is actually moving.

A surface painted under a scale (a thumbnail, a picture zooming in) gets
`cairo_pattern_set_filter(..., CAIRO_FILTER_BILINEAR)`. cairo's default `GOOD` filter downscales
through a convolution it rebuilds for every new scale factor, and a scale that changes every frame
rebuilds it on every frame. That is what held the server's open animation at 14 frames a second
before it switched. Under an identity transform bilinear costs nothing, so there is no reason to
switch back between frames.

Stop when nobody is looking. A caret stops blinking in a window that is not the active one, and a
loop that runs forever (a pulse, a spinner) should do the same: it is the difference between an
idle desktop and one waking sixty times a second for each window on it.

## Inside a widget

A widget kind animates through its `tick` hook, which `ui_view_timeout()` calls with the clock
reading; see [Adding a widget](widgets.md#adding-a-widget). Two internal helpers read its
transitions against the frame being painted rather than the clock:

```c
bool   ui_widget_animating(const ui_widget_t* widget, const ui_anim_t* anim);
double ui_widget_anim(const ui_widget_t* widget, const ui_anim_t* anim);
```

`ui_widget_animating()` is true while the last frame painted has not yet shown the end of the
transition, which is what makes a transition that ran out between two frames still get its last
frame. `ui_widget_anim()` is the value at that frame's time. Both live in
`ui_widget_internal.h`, which is not installed; an application gets the same effect from the
public calls by reading the clock once per pass and settling what has finished, as
[the tutorial](tutorial-animation.md) does.
