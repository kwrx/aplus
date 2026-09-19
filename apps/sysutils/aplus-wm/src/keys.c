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

/**
 * @brief Global keybindings, caught before the key reaches the focused window.
 *
 * The bindings are on raw key codes, so a binding is a physical key rather than a letter.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <aplus/input.h>

#include <wm.h>


static const char* const wm_command_terminal[] = {"/usr/bin/aplus-terminal", NULL};
static const char* const wm_command_explorer[] = {"/usr/bin/aplus-explorer", NULL};


typedef enum {

    WM_ACTION_SPAWN,
    WM_ACTION_CLOSE,
    WM_ACTION_CYCLE_NEXT,
    WM_ACTION_CYCLE_PREV,

} wm_action_t;


/**
 * @brief The bindings, each matching on the exact set of modifiers held.
 */
static const struct {

    uint16_t modifiers;
    uint16_t vkey;

    wm_action_t action;
    const char* const* argv;

} wm_bindings[] = {

    {WM_MOD_CTRL | WM_MOD_ALT,  KEY_T,   WM_ACTION_SPAWN,      wm_command_terminal},
    {WM_MOD_CTRL | WM_MOD_ALT,  KEY_E,   WM_ACTION_SPAWN,      wm_command_explorer},
    {WM_MOD_CTRL | WM_MOD_ALT,  KEY_Q,   WM_ACTION_CLOSE,      NULL               },
    {WM_MOD_ALT,                KEY_TAB, WM_ACTION_CYCLE_NEXT, NULL               },
    {WM_MOD_ALT | WM_MOD_SHIFT, KEY_TAB, WM_ACTION_CYCLE_PREV, NULL               },
};


/**
 * @brief One bit per key code, set for a key whose press a binding consumed, so its release is consumed too.
 */
static uint32_t wm_swallowed[(KEY_CNT + 31) / 32] = {0};


static uint16_t wm_modifier_of(uint16_t vkey) {

    switch (vkey) {

        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            return WM_MOD_SHIFT;

        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            return WM_MOD_CTRL;

        case KEY_LEFTALT:
        case KEY_RIGHTALT:
            return WM_MOD_ALT;

        case KEY_LEFTMETA:
        case KEY_RIGHTMETA:
            return WM_MOD_SUPER;

        default:
            return 0;
    }
}


static void wm_spawn(const char* const* argv) {

    pid_t pid = fork();

    if (pid < 0) {

        fprintf(stderr, "aplus-wm: fork() failed: %s\n", strerror(errno));
        return;
    }

    if (pid > 0) {

        fprintf(stderr, "aplus-wm: spawned %s as pid %d\n", argv[0], pid);
        return;
    }


    for (int fd = STDERR_FILENO + 1; fd < CONFIG_OPEN_MAX; fd++) {
        close(fd);
    }

    execv(argv[0], (char* const*)argv);

    _exit(127);
}


/**
 * @brief The window order Alt+Tab walks, held from the first Tab until Alt comes up.
 *
 * Ids rather than pointers, so a window closed mid-walk is skipped.
 */

#define WM_CYCLE_MAX 64

static struct {

    bool active;

    size_t count;
    size_t index;

    uint32_t ids[WM_CYCLE_MAX];

} wm_cycle;


/**
 * @brief Takes the stacking order as it stands, anchored on the focused window.
 */
static void wm_cycle_begin(void) {

    wm_cycle.count = 0;
    wm_cycle.index = 0;

    for (wm_window_t* win = wm.windows; win && wm_cycle.count < WM_CYCLE_MAX; win = win->next) {

        if (win == wm.focused) {
            wm_cycle.index = wm_cycle.count;
        }

        wm_cycle.ids[wm_cycle.count++] = win->id;
    }

    wm_cycle.active = true;
}


/**
 * @brief One step of the walk, raising and focusing what it lands on.
 *
 * @param forward Whether to step towards the back of the stack or the front.
 */
static void wm_cycle_step(bool forward) {

    const uint32_t focused = wm.focused ? wm.focused->id : 0;

    if (!wm_cycle.active || wm_cycle.index >= wm_cycle.count || wm_cycle.ids[wm_cycle.index] != focused) {
        wm_cycle_begin();
    }


    const size_t step = !wm.focused ? 0 : (forward ? 1 : wm_cycle.count - 1);

    for (size_t i = 0; i < wm_cycle.count; i++) {

        wm_cycle.index = (wm_cycle.index + step) % wm_cycle.count;

        wm_window_t* win = wm_window_from_id(wm_cycle.ids[wm_cycle.index]);

        if (!win) {
            continue;
        }

        wm_window_raise(win);
        wm_window_focus(win);

        return;
    }
}


/**
 * @brief Acts on a key event, before the focused client sees it.
 *
 * @param vkey The key code.
 * @param down Whether the key went down or came up.
 * @return true when the key was a binding and must not reach the client.
 */
bool wm_keys_handle(uint16_t vkey, uint8_t down) {

    const uint16_t modifier = wm_modifier_of(vkey);

    if (modifier) {

        if (down) {
            wm.keyboard.modifiers |= modifier;
        } else {

            wm.keyboard.modifiers &= (uint16_t)~modifier;

            if (modifier == WM_MOD_ALT) {
                wm_cycle.active = false;
            }
        }

        return false;
    }


    if (vkey >= KEY_CNT) {
        return false;
    }


    const size_t word  = vkey / 32;
    const uint32_t bit = 1U << (vkey % 32);

    if (!down) {

        if (!(wm_swallowed[word] & bit)) {
            return false;
        }

        wm_swallowed[word] &= ~bit;
        return true;
    }

    if (wm_swallowed[word] & bit) {
        return true;
    }


    for (size_t i = 0; i < WM_ARRAY_COUNT(wm_bindings); i++) {

        if (wm_bindings[i].vkey != vkey) {
            continue;
        }

        if (wm_bindings[i].modifiers != wm.keyboard.modifiers) {
            continue;
        }


        wm_swallowed[word] |= bit;

        switch (wm_bindings[i].action) {

            case WM_ACTION_SPAWN:
                wm_spawn(wm_bindings[i].argv);
                break;

            case WM_ACTION_CLOSE:
                wm_window_request_close(wm.focused);
                break;

            case WM_ACTION_CYCLE_NEXT:
                wm_cycle_step(true);
                break;

            case WM_ACTION_CYCLE_PREV:
                wm_cycle_step(false);
                break;
        }

        return true;
    }

    return false;
}


/**
 * @brief Reaps the zombies left by spawned clients, which nothing else waits for.
 */
void wm_keys_reap(void) {

    while (waitpid(-1, NULL, WNOHANG) > 0) {
        ;
    }
}
