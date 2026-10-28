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

#include <time.h>
#include <math.h>

#include <aplus/ui-anim.h>


uint64_t ui_anim_now(void) {

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }

    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000L);
}


double ui_ease(ui_ease_t ease, double t) {

    if (t <= 0.0) {
        return 0.0;
    }

    if (t >= 1.0) {
        return 1.0;
    }


    switch (ease) {

        case UI_EASE_IN:
            return t * t * t;

        case UI_EASE_IN_OUT:
            return t < 0.5 ? 4.0 * t * t * t : 1.0 - (-2.0 * t + 2.0) * (-2.0 * t + 2.0) * (-2.0 * t + 2.0) / 2.0;

        case UI_EASE_LINEAR:
            return t;
        
        case UI_EASE_OUT_CUBIC:
            return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);

        case UI_EASE_OUT_QUINT:
            return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t) * (1.0 - t) * (1.0 - t);

        case UI_EASE_OUT:
        case UI_EASE_OUT_QUART:
        default:
            return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t) * (1.0 - t);
    }
}


double ui_lerp(double a, double b, double t) {
    return a + (b - a) * t;
}


void ui_anim_set(ui_anim_t* anim, double value) {

    anim->from     = value;
    anim->to       = value;
    anim->start    = 0;
    anim->duration = 0;
}


void ui_anim_to(ui_anim_t* anim, double target, uint32_t duration, uint64_t now) {

    if (anim->to == target) {
        return;
    }

    if (duration == 0) {
        ui_anim_set(anim, target);
        return;
    }

    anim->from     = ui_anim_value(anim, now);
    anim->to       = target;
    anim->start    = now;
    anim->duration = duration;
}


double ui_anim_value(const ui_anim_t* anim, uint64_t now) {

    if (anim->duration == 0 || now >= anim->start + anim->duration) {
        return anim->to;
    }

    if (now <= anim->start) {
        return anim->from;
    }

    return ui_lerp(anim->from, anim->to, ui_ease(anim->ease, (double)(now - anim->start) / (double)anim->duration));
}


double ui_anim_target(const ui_anim_t* anim) {
    return anim->to;
}


bool ui_anim_running(const ui_anim_t* anim, uint64_t now) {
    return anim->duration != 0 && now < anim->start + anim->duration;
}
