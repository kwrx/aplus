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

/*
 * Global keybindings.
 *
 * A chord is caught here before the key reaches the focused window, which is what makes it
 * global: it fires whichever client has the keyboard, and it fires when none of them does.
 *
 * The bindings are on raw key codes, because raw key codes are all the server has -- the
 * keymap lives in the client, which is where it belongs while there is no toolkit. That
 * means a binding is a physical key rather than a letter, so it lands in the same place on
 * every layout instead of moving with it.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <aplus/input.h>

#include <wm.h>


static const char* const wm_command_terminal[] = {"/usr/bin/aplus-terminal", NULL};


typedef enum {

    WM_ACTION_SPAWN,
    WM_ACTION_CLOSE,

} wm_action_t;


/* The table. A binding matches on the exact set of modifiers held, so a chord with an extra
   modifier on it falls through to the client rather than firing something the user did not
   ask for. */
static const struct {

    uint16_t modifiers;
    uint16_t vkey;

    wm_action_t action;
    const char* const* argv;

} wm_bindings[] = {

    {WM_MOD_CTRL | WM_MOD_ALT, KEY_T, WM_ACTION_SPAWN, wm_command_terminal},
    {WM_MOD_CTRL | WM_MOD_ALT, KEY_Q, WM_ACTION_CLOSE, NULL               },
};


/* One bit per key code, set for a key whose press a binding consumed. It does two jobs: the
   release is consumed along with the press, so the focused client never sees a key go up
   that it never saw come down, and a held key repeating at the keyboard's own rate fires
   the binding once instead of once per repeat. */
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


    /* Everything above the standard streams belongs to the server: the framebuffer, the
     * listening socket, the input pipe, and one accepted connection per client already
     * running. A process that inherited those would hold another client's socket open long
     * after the server had dropped it, so that client would never learn its window was
     * gone -- the same fd leak that once kept a closed terminal's window on screen, only
     * seen from the other end.
     *
     * Nothing between the fork and the exec allocates. This is a forked child of a threaded
     * process, and the input threads are not here to release the malloc lock if one of them
     * happened to be holding it. */
    for (int fd = STDERR_FILENO + 1; fd < CONFIG_OPEN_MAX; fd++) {
        close(fd);
    }

    execv(argv[0], (char* const*)argv);

    _exit(127);
}


/* Returns true when the key was a binding and must not reach the client. */
bool wm_keys_handle(uint16_t vkey, uint8_t down) {

    const uint16_t modifier = wm_modifier_of(vkey);

    if (modifier) {

        if (down) {
            wm.keyboard.modifiers |= modifier;
        } else {
            wm.keyboard.modifiers &= (uint16_t)~modifier;
        }

        /* A modifier is never swallowed: the client tracks shift for its own keymap, and a
           chord that ate the release would leave it shifted forever. */
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


    for (size_t i = 0; i < sizeof(wm_bindings) / sizeof(wm_bindings[0]); i++) {

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

            /* The same request the close button makes, aimed at whatever holds the
               keyboard. With nothing focused there is nothing to ask, and the chord is
               still eaten -- it matched, it simply had no window to act on. */
            case WM_ACTION_CLOSE:
                wm_window_request_close(wm.focused);
                break;
        }

        return true;
    }

    return false;
}


/* Nothing reparents an orphan to init here and SIGCHLD is not implemented, so a spawned
   client that exits stays a zombie in the scheduler's queue until someone waits for it. The
   threads reading the input devices are not matched: a CLONE_THREAD task inherits its
   creator's parent rather than becoming its child, so wait4() walks straight past them. */
void wm_keys_reap(void) {

    while (waitpid(-1, NULL, WNOHANG) > 0) {
        ;
    }
}
