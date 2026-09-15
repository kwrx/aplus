/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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
 * Smoke test for libui and the aplus-wm wire protocol.
 *
 * Opens a window, paints a gradient into it, and prints every event it receives. It
 * exists so that the server and the library can be debugged without dragging the whole
 * terminal port in: if this shows a window and reports keys, the protocol works.
 *
 * With --once it paints a single frame and exits. A client that lives forever cannot be driven
 * from a script, and the lifecycle is the interesting thing to drive: every window costs a
 * shared memory segment that the server creates and both ends have to let go of, so a loop of
 * these is what shows a leak.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <aplus/ui.h>


static void paint(ui_window_t* win) {

    uint8_t* pixels = (uint8_t*)ui_window_pixels(win);

    const int w         = ui_window_width(win);
    const int h         = ui_window_height(win);
    const size_t stride = ui_window_stride(win);

    for (int y = 0; y < h; y++) {

        uint32_t* row = (uint32_t*)(pixels + ((size_t)y * stride));

        for (int x = 0; x < w; x++) {

            uint8_t r = (uint8_t)((x * 255) / (w > 1 ? w - 1 : 1));
            uint8_t g = (uint8_t)((y * 255) / (h > 1 ? h - 1 : 1));
            uint8_t b = 0x80;

            row[x] = 0xFF000000U | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }

    ui_window_damage_all(win);
}


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);


    bool once = false;

    int width  = 480;
    int height = 320;

    int positional = 0;

    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "--once") == 0) {
            once = true;
            continue;
        }

        switch (positional++) {

            case 0:
                width = atoi(argv[i]);
                break;

            case 1:
                height = atoi(argv[i]);
                break;
        }
    }


    ui_connection_t* conn = ui_connect(NULL, 5000);

    if (!conn) {
        fprintf(stderr, "ui-test: ui_connect() failed: %s\n", strerror(errno));
        return 1;
    }

    printf("ui-test: connected to " UI_DEFAULT_SOCKET "\n");


    ui_window_t* win = ui_window_create(conn, width, height, "ui-test");

    if (!win) {
        fprintf(stderr, "ui-test: ui_window_create() failed: %s\n", strerror(errno));
        return 1;
    }

    printf("ui-test: window %u is %dx%d\n", ui_window_id(win), ui_window_width(win), ui_window_height(win));


    paint(win);

    if (ui_window_commit(win) < 0) {
        fprintf(stderr, "ui-test: ui_window_commit() failed: %s\n", strerror(errno));
        return 1;
    }


    if (once) {

        ui_window_destroy(win);
        ui_disconnect(conn);

        printf("ui-test: painted one frame\n");

        return 0;
    }


    for (;;) {

        ui_event_t ev;

        int e = ui_next_event(conn, &ev, -1);

        if (e < 0) {
            fprintf(stderr, "ui-test: ui_next_event() failed: %s\n", strerror(errno));
            break;
        }

        if (e == 0) {
            continue;
        }


        switch (ev.type) {

            case UI_EVENT_KEY:
                printf("ui-test: key vkey=%u %s\n", ev.key.vkey, ev.key.down ? "down" : "up");
                break;

            case UI_EVENT_POINTER:
                printf("ui-test: pointer x=%d y=%d buttons=%02x\n", ev.pointer.x, ev.pointer.y, ev.pointer.buttons);
                break;

            case UI_EVENT_FOCUS:
                printf("ui-test: focus %s\n", ev.focus.focused ? "in" : "out");
                break;

            case UI_EVENT_CONFIGURE:
                printf("ui-test: configure %ux%u serial=%u\n", ev.configure.width, ev.configure.height, ev.configure.serial);

                if (ui_window_apply_configure(win) < 0) {
                    fprintf(stderr, "ui-test: ui_window_apply_configure() failed: %s\n", strerror(errno));
                    break;
                }

                paint(win);

                if (ui_window_commit(win) < 0) {
                    fprintf(stderr, "ui-test: ui_window_commit() failed: %s\n", strerror(errno));
                }

                break;

            case UI_EVENT_CLOSE:
                printf("ui-test: close\n");
                ui_window_destroy(win);
                ui_disconnect(conn);
                return 0;

            default:
                break;
        }
    }


    ui_window_destroy(win);
    ui_disconnect(conn);

    return 1;
}
