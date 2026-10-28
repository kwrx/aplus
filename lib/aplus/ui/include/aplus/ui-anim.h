/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _APLUS_UI_ANIM_H
#define _APLUS_UI_ANIM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief How often a running animation asks to be painted, in milliseconds.
 */
#define UI_ANIM_FRAME_MS 16

/**
 * @brief The durations the toolkit and the server share, so that every transition moves at one pace.
 */
#define UI_MOTION_FAST    110
#define UI_MOTION_NORMAL  180
#define UI_MOTION_SLOW    240


/**
 * @brief The curve a transition follows from its start to its target.
 */
typedef enum {

    UI_EASE_OUT = 0,
    UI_EASE_IN,
    UI_EASE_IN_OUT,
    UI_EASE_LINEAR,
    UI_EASE_OUT_CUBIC,
    UI_EASE_OUT_QUART,
    UI_EASE_OUT_QUINT,

} ui_ease_t;


/**
 * @brief One value in transition, which a zero-initialised struct holds still at 0.
 */
typedef struct {

    double from;
    double to;

    uint64_t start;
    uint32_t duration;

    ui_ease_t ease;

} ui_anim_t;


/**
 * @brief Reads the monotonic clock that every transition is timed against.
 *
 * @return Milliseconds since an arbitrary origin, or 0 when there is no clock.
 */
uint64_t ui_anim_now(void);

/**
 * @brief Shapes a linear progress along a curve.
 *
 * @param ease The curve.
 * @param t The progress, clamped to 0..1.
 * @return The eased progress, 0..1.
 */
double ui_ease(ui_ease_t ease, double t);

/**
 * @brief Holds a value still, cutting short whatever transition it was in.
 *
 * @param anim The animation.
 * @param value The value to hold.
 */
void ui_anim_set(ui_anim_t* anim, double value);

/**
 * @brief Starts a transition towards a target from wherever the value is right now.
 *
 * @param anim The animation.
 * @param target The value to end at.
 * @param duration How long to take, in milliseconds; 0 jumps straight there.
 * @param now The clock reading the transition starts at.
 */
void ui_anim_to(ui_anim_t* anim, double target, uint32_t duration, uint64_t now);

/**
 * @brief Reports the value at a moment.
 *
 * @param anim The animation.
 * @param now The clock reading.
 * @return The eased value.
 */
double ui_anim_value(const ui_anim_t* anim, uint64_t now);

/**
 * @brief Reports the value a transition is heading for.
 *
 * @param anim The animation.
 * @return The target.
 */
double ui_anim_target(const ui_anim_t* anim);

/**
 * @brief Reports whether a transition is still under way at a moment.
 *
 * @param anim The animation.
 * @param now The clock reading.
 * @return true until the duration has run out.
 */
bool ui_anim_running(const ui_anim_t* anim, uint64_t now);

/**
 * @brief Interpolates linearly between two values.
 *
 * @param a The value at 0.
 * @param b The value at 1.
 * @param t The position between them.
 * @return The interpolated value.
 */
double ui_lerp(double a, double b, double t);


#ifdef __cplusplus
}
#endif

#endif
