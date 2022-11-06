/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2022 Antonino Natale
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

#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/times.h>

#include <wc/wc.h>
#include <wc/wc_display.h>
#include <wc/wc_cursor.h>
#include <wc/wc_font.h>
#include <wc/wc_renderer.h>
#include <wc/wc_window.h>
#include <wc/wc_input.h>
#include <wc/wc_event.h>

#include <aplus/events.h>


static void show_usage(int argc, char** argv) {
    printf(
        "Use: aplus-ui [options]...\n"
        "User Interface Server.\n\n"
        "Options:\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );

    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aplus coreutils) 0.1\n"
        "Copyright (c) %s Antonino Natale.\n"
        "Built with gcc %s (%s)\n",

        argv[0], __DATE__ + 7, __VERSION__, __TIMESTAMP__
    );

    exit(0);
}




int main(int argc, char** argv) {


    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };



    int c, idx;
    while((c = getopt_long(argc, argv, "vh", long_options, &idx)) != -1) {
        switch(c) {
            case 'v':
                show_version(argc, argv);
                break;
            case 'h':
            case '?':
                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }




    if(wc_event_initialize() < 0) {
        return fprintf(stderr, "wc_event_initialize() failed: %s\n", strerror(errno)), 1;
    }

    if(wc_input_initialize() < 0) {
        return fprintf(stderr, "wc_input_initialize() failed: %s\n", strerror(errno)), 1;
    }

    if(wc_font_initialize() < 0) {
        return fprintf(stderr, "wc_font_initialize() failed: %s\n", strerror(errno)), 1;
    }

    if(wc_cursor_initialize() < 0) {
        return fprintf(stderr, "wc_cursor_initialize() failed: %s\n", strerror(errno)), 1;
    }

    if(wc_display_initialize() < 0) {
        return fprintf(stderr, "wc_display_initialize() failed: %s\n", strerror(errno)), 1;
    }




    wc_cursor_load(WC_CURSOR_TYPE_POINTER, "/usr/share/cursors/arrow.webp");
    wc_cursor_load(WC_CURSOR_TYPE_TEXT,    "/usr/share/cursors/text.webp");
    wc_cursor_load(WC_CURSOR_TYPE_HAND,    "/usr/share/cursors/hand.webp");
    wc_cursor_load(WC_CURSOR_TYPE_HELP,    "/usr/share/cursors/help.webp");
    wc_cursor_load(WC_CURSOR_TYPE_MOVE,    "/usr/share/cursors/cross.webp");
    wc_cursor_load(WC_CURSOR_TYPE_RESIZE,  "/usr/share/cursors/size_all.webp");

    wc_cursor_set_fallback(WC_CURSOR_TYPE_POINTER);
    wc_cursor_set_type(WC_CURSOR_TYPE_POINTER);



    wc_renderer_t* renderer;
    if(wc_renderer_create(&renderer, wc_display_primary()) < 0) {
        return fprintf(stderr, "wc_renderer_create() failed: %s\n", strerror(errno)), 1;
    }


    wc_renderer_clear(renderer, 1.0, 1.0, 1.0);
    wc_renderer_flush(renderer);



    



    wc_font_t* font;
    if(wc_font_from_family(&font, "Ubuntu") < 0) {
        return fprintf(stderr, "wc_font_from_family() failed: %s\n", strerror(errno)), 1;
    }






    int counter = 0;
    int current = 0;
    int frametime = 0;
    int last_time = time(NULL);


    wc_window_t* window1;
    wc_window_create(&window1, NULL);
    wc_window_set_title(window1, "Window 1");
    wc_window_set_position(window1, 250, 250);
    wc_window_set_size(window1, 400, 400);
    wc_window_set_flags(window1, WC_WINDOW_FLAGS_RESIZABLE);
    wc_window_set_font(window1, font);

    wc_window_t* window2;
    wc_window_create(&window2, NULL);
    wc_window_set_title(window2, "Window 2");
    wc_window_set_position(window2, 200, 200);
    wc_window_set_size(window2, 400, 400);
    wc_window_set_flags(window2, WC_WINDOW_FLAGS_RESIZABLE);
    wc_window_set_font(window2, font);

    wc_window_t* window3;
    wc_window_create(&window3, NULL);
    wc_window_set_title(window3, "Window 3");
    wc_window_set_position(window3, 300, 300);
    wc_window_set_size(window3, 400, 400);
    wc_window_set_flags(window3, WC_WINDOW_FLAGS_RESIZABLE);
    wc_window_set_font(window3, font);


    do {

        // TODO: cursor subsystem
        // TODO: renderer subsystem
        // TODO: window manager subsystem
        // TODO: message subsystem


        // get current time in us
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        int now = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;

        wc_renderer_clear(renderer, 0, 0, 0);

        char fps[32] = { 0 };
        sprintf(fps, "%d: %g ms", current, (double) frametime / 1000.0);

        cairo_save(renderer->cr);
        cairo_set_font_face(renderer->cr, font->face);
        cairo_set_font_size(renderer->cr, 36.0);
        cairo_set_source_rgb(renderer->cr, 1.0, 1.0, 1.0);
        cairo_move_to(renderer->cr, 50, 50);
        cairo_show_text(renderer->cr, fps);
        cairo_restore(renderer->cr);


        wc_window_draw_all(renderer);
        wc_renderer_flush(renderer);

        counter++;

        if(time(NULL) != last_time) {
            last_time = time(NULL);
            current = counter;
            counter = 0;
        }

        clock_gettime(CLOCK_MONOTONIC, &ts);
        int then = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;

        frametime = then - now;

    } while(wc_event_wait(NULL) == 0);


    return 0;

}