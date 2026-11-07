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

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aplus/input.h>
#include <aplus/ui-anim.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>
#include <aplus/ulog.h>

#include "player.h"


#define PLAYER_NAME "Music Player"

#define PLAYER_WINDOW_WIDTH  480
#define PLAYER_WINDOW_HEIGHT 216

#define PLAYER_MARGIN      16
#define PLAYER_GAP         8
#define PLAYER_ART         80
#define PLAYER_ART_ICON    48
#define PLAYER_SEEK_HEIGHT 24
#define PLAYER_BUTTON      40
#define PLAYER_BUTTON_MAIN 48
#define PLAYER_ICON        24
#define PLAYER_ICON_MAIN   32
#define PLAYER_TIME_WIDTH  52
#define PLAYER_VOLUME      120

#define PLAYER_SEEK_STEP    5.0
#define PLAYER_SKIP_SECONDS 10.0
#define PLAYER_VOLUME_STEP  5
#define PLAYER_VOLUME_START 100

#define PLAYER_FEED_MS    40
#define PLAYER_REFRESH_MS 100
#define PLAYER_DRAIN_MAX  64

#define PLAYER_TEXT_MAX 128


typedef enum {

    PLAYER_STOPPED = 0,
    PLAYER_PLAYING,
    PLAYER_PAUSED,

} player_state_t;


static struct {

    ui_connection_t* conn;
    ui_window_t* window;
    ui_view_t* view;

    ui_widget_t* art;
    ui_widget_t* image;
    ui_widget_t* title;
    ui_widget_t* details;
    ui_widget_t* status;

    ui_widget_t* elapsed;
    ui_widget_t* seek;
    ui_widget_t* total;

    ui_widget_t* back;
    ui_widget_t* play;
    ui_widget_t* stop;
    ui_widget_t* forward;

    ui_widget_t* mute;
    ui_widget_t* volume;

} player;


static struct {

    player_state_t state;

    bool loaded;
    const char* name;

    player_source_t source;
    player_output_t output;

    uint64_t position;
    uint64_t anchor;

    int volume;
    bool muted;

    const char* error;

    uint64_t refreshed;

} playback = {

    .source = {.fd = -1},
    .output = {.fd = -1},
    .volume = PLAYER_VOLUME_START,
};


static void player_format_time(double seconds, char* out, size_t size) {

    if (seconds < 0.0) {
        seconds = 0.0;
    }


    const unsigned long total = (unsigned long)seconds;

    if (total >= 3600) {
        snprintf(out, size, "%lu:%02lu:%02lu", total / 3600, (total / 60) % 60, total % 60);
    } else {
        snprintf(out, size, "%lu:%02lu", total / 60, total % 60);
    }
}


static double player_seconds(uint64_t frames) {

    if (!playback.loaded || playback.source.rate == 0) {
        return 0.0;
    }

    return (double)frames / playback.source.rate;
}


static uint64_t player_position(void) {

    if (playback.state != PLAYER_PLAYING || playback.output.fd < 0) {
        return playback.position;
    }


    const uint64_t heard    = player_output_heard(&playback.output);
    const uint64_t position = playback.anchor + heard * playback.source.rate / playback.output.rate;

    return position < playback.source.frames ? position : playback.source.frames;
}


static uint32_t player_gain(void) {

    if (playback.muted) {
        return 0;
    }

    return (uint32_t)((uint64_t)PLAYER_GAIN_UNITY * (uint64_t)(playback.volume * playback.volume) / 10000);
}


static void player_restyle(ui_view_t* view, void* user) {

    (void)view;
    (void)user;

    ui_panel_set_color(player.art, ui_theme()->surface_sunken);
    ui_panel_set_border(player.art, ui_theme()->border, 1.0);

    ui_label_set_color(player.title, ui_theme()->text);
    ui_label_set_color(player.details, ui_theme()->text_muted);
    ui_label_set_color(player.status, playback.error ? ui_theme()->danger : ui_theme()->text_muted);

    ui_label_set_color(player.elapsed, ui_theme()->text_muted);
    ui_label_set_color(player.total, ui_theme()->text_muted);
}


static void player_show_time(double seconds) {

    char text[32];

    player_format_time(seconds, text, sizeof(text));
    ui_label_set_text(player.elapsed, text);
}


static void player_refresh(void) {

    const uint64_t position = player_position();

    if (!ui_slider_dragging(player.seek)) {

        player_show_time(player_seconds(position));
        ui_slider_set_value(player.seek, player_seconds(position));
    }


    char text[PLAYER_TEXT_MAX];

    player_format_time(player_seconds(playback.source.frames), text, sizeof(text));
    ui_label_set_text(player.total, text);


    const char* status;

    if (playback.error) {
        status = playback.error;
    } else if (!playback.loaded) {
        status = "Open a WAV or PCM file from the file manager";
    } else if (playback.state == PLAYER_PLAYING) {
        status = "Playing";
    } else if (playback.state == PLAYER_PAUSED) {
        status = "Paused";
    } else {
        status = "Stopped";
    }

    ui_label_set_text(player.status, status);
    ui_label_set_color(player.status, playback.error ? ui_theme()->danger : ui_theme()->text_muted);


    ui_button_set_icon(player.play, playback.state == PLAYER_PLAYING ? "media-playback-pause" : "media-playback-start");
    ui_button_set_icon(player.mute, playback.muted || playback.volume == 0 ? "audio-volume-muted" : "audio-volume-high");

    ui_widget_set_enabled(player.play, playback.loaded);
    ui_widget_set_enabled(player.stop, playback.loaded && playback.state != PLAYER_STOPPED);
    ui_widget_set_enabled(player.back, playback.loaded);
    ui_widget_set_enabled(player.forward, playback.loaded);
    ui_widget_set_enabled(player.seek, playback.loaded && playback.source.frames > 0);

    playback.refreshed = ui_anim_now();
}


static void player_fail(const char* error) {

    playback.position = player_position();

    player_output_close(&playback.output);

    playback.state = playback.position > 0 ? PLAYER_PAUSED : PLAYER_STOPPED;
    playback.error = error;

    ulog_errorf("%s: %s", playback.name ? playback.name : PLAYER_NAME, error);
}


static void player_play(void) {

    if (!playback.loaded || playback.state == PLAYER_PLAYING) {
        return;
    }

    if (playback.position >= playback.source.frames) {
        playback.position = 0;
    }


    const char* error = NULL;

    if (player_output_open(&playback.output, playback.source.rate, &error) < 0) {
        playback.error = error;
        player_refresh();
        return;
    }

    if (player_source_seek(&playback.source, playback.position) < 0) {
        player_output_close(&playback.output);
        playback.error = strerror(errno);
        player_refresh();
        return;
    }

    playback.anchor = playback.position;
    playback.state  = PLAYER_PLAYING;
    playback.error  = NULL;

    player_refresh();
}


static void player_pause(void) {

    if (playback.state != PLAYER_PLAYING) {
        return;
    }

    playback.position = player_position();

    player_output_close(&playback.output);

    playback.state = PLAYER_PAUSED;

    player_refresh();
}


static void player_stop(void) {

    player_output_close(&playback.output);

    playback.position = 0;
    playback.state    = PLAYER_STOPPED;

    player_refresh();
}


static void player_toggle(void) {

    if (playback.state == PLAYER_PLAYING) {
        player_pause();
    } else {
        player_play();
    }
}


static void player_seek(double seconds) {

    if (!playback.loaded) {
        return;
    }


    double frame = seconds * playback.source.rate;

    if (frame < 0.0) {
        frame = 0.0;
    }

    if (frame > (double)playback.source.frames) {
        frame = (double)playback.source.frames;
    }

    playback.position = (uint64_t)frame;

    if (playback.state == PLAYER_PLAYING) {

        player_output_reset(&playback.output);

        if (player_source_seek(&playback.source, playback.position) < 0) {
            player_fail(strerror(errno));
        }

        playback.anchor = playback.position;

    } else if (playback.state == PLAYER_STOPPED && playback.position > 0) {

        playback.state = PLAYER_PAUSED;
    }

    player_refresh();
}


static void player_skip(double seconds) {

    if (playback.loaded) {
        player_seek(player_seconds(player_position()) + seconds);
    }
}


static void player_set_volume(int volume) {

    if (volume < 0) {
        volume = 0;
    }

    if (volume > 100) {
        volume = 100;
    }

    playback.volume = volume;
    playback.muted  = false;

    ui_slider_set_value(player.volume, volume);

    player_refresh();
}


static void player_service(void) {

    if (playback.state != PLAYER_PLAYING) {
        return;
    }

    if (player_output_fill(&playback.output, &playback.source, player_gain()) < 0) {
        player_fail(strerror(errno));
        player_refresh();
        return;
    }

    if (player_output_done(&playback.output)) {
        player_stop();
        return;
    }

    if (ui_anim_now() - playback.refreshed >= PLAYER_REFRESH_MS) {
        player_refresh();
    }
}


static void player_on_play(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    player_toggle();
}


static void player_on_stop(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    player_stop();
}


static void player_on_back(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    player_skip(-PLAYER_SKIP_SECONDS);
}


static void player_on_forward(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    player_skip(PLAYER_SKIP_SECONDS);
}


static void player_on_mute(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    if (playback.volume == 0) {
        player_set_volume(PLAYER_VOLUME_START);
        return;
    }

    playback.muted = !playback.muted;

    player_refresh();
}


static void player_on_seek_change(ui_widget_t* widget, double value, void* user) {

    (void)user;

    if (ui_slider_dragging(widget)) {
        player_show_time(value);
    }
}


static void player_on_seek_commit(ui_widget_t* widget, double value, void* user) {

    (void)widget;
    (void)user;

    player_seek(value);
}


static void player_on_volume(ui_widget_t* widget, double value, void* user) {

    (void)widget;
    (void)user;

    playback.volume = (int)(value + 0.5);
    playback.muted  = false;

    player_refresh();
}


static bool player_on_key(ui_view_t* view, uint16_t vkey, bool down, void* user) {

    (void)view;
    (void)user;

    if (!down) {
        return false;
    }


    switch (vkey) {

        case KEY_SPACE:
        case KEY_K:
            player_toggle();
            return true;

        case KEY_LEFT:
            player_skip(-PLAYER_SEEK_STEP);
            return true;

        case KEY_RIGHT:
            player_skip(PLAYER_SEEK_STEP);
            return true;

        case KEY_HOME:
            player_seek(0.0);
            return true;

        case KEY_UP:
            player_set_volume(playback.volume + PLAYER_VOLUME_STEP);
            return true;

        case KEY_DOWN:
            player_set_volume(playback.volume - PLAYER_VOLUME_STEP);
            return true;

        case KEY_M:
            player_on_mute(NULL, NULL);
            return true;

        default:
            return false;
    }
}


static void player_layout(ui_view_t* view, int width, int height, void* user) {

    (void)view;
    (void)user;


    const int right = width - PLAYER_MARGIN;

    ui_widget_set_rect(player.art, PLAYER_MARGIN, PLAYER_MARGIN, PLAYER_ART, PLAYER_ART);
    ui_widget_set_rect(player.image, PLAYER_MARGIN, PLAYER_MARGIN, PLAYER_ART, PLAYER_ART);


    const int text_x = PLAYER_MARGIN + PLAYER_ART + 2 * PLAYER_GAP;
    const int text_w = right - text_x;

    ui_widget_set_rect(player.title, text_x, PLAYER_MARGIN + 4, text_w, 26);
    ui_widget_set_rect(player.details, text_x, PLAYER_MARGIN + 32, text_w, 20);
    ui_widget_set_rect(player.status, text_x, PLAYER_MARGIN + 54, text_w, 20);


    const int controls_y = height - PLAYER_MARGIN - PLAYER_BUTTON_MAIN;
    const int seek_y     = controls_y - PLAYER_GAP - PLAYER_SEEK_HEIGHT - 4;

    ui_widget_set_rect(player.elapsed, PLAYER_MARGIN, seek_y, PLAYER_TIME_WIDTH, PLAYER_SEEK_HEIGHT);
    ui_widget_set_rect(player.seek, PLAYER_MARGIN + PLAYER_TIME_WIDTH, seek_y, right - PLAYER_MARGIN - 2 * PLAYER_TIME_WIDTH, PLAYER_SEEK_HEIGHT);
    ui_widget_set_rect(player.total, right - PLAYER_TIME_WIDTH, seek_y, PLAYER_TIME_WIDTH, PLAYER_SEEK_HEIGHT);


    const int small_y = controls_y + (PLAYER_BUTTON_MAIN - PLAYER_BUTTON) / 2;

    int x = PLAYER_MARGIN;

    ui_widget_set_rect(player.back, x, small_y, PLAYER_BUTTON, PLAYER_BUTTON);
    x += PLAYER_BUTTON + PLAYER_GAP;

    ui_widget_set_rect(player.play, x, controls_y, PLAYER_BUTTON_MAIN, PLAYER_BUTTON_MAIN);
    x += PLAYER_BUTTON_MAIN + PLAYER_GAP;

    ui_widget_set_rect(player.stop, x, small_y, PLAYER_BUTTON, PLAYER_BUTTON);
    x += PLAYER_BUTTON + PLAYER_GAP;

    ui_widget_set_rect(player.forward, x, small_y, PLAYER_BUTTON, PLAYER_BUTTON);


    ui_widget_set_rect(player.volume, right - PLAYER_VOLUME, small_y, PLAYER_VOLUME, PLAYER_BUTTON);
    ui_widget_set_rect(player.mute, right - PLAYER_VOLUME - PLAYER_BUTTON, small_y, PLAYER_BUTTON, PLAYER_BUTTON);
}


static ui_widget_t* player_icon_button(ui_view_t* view, const char* icon, int size, ui_action_fn fn) {

    ui_widget_t* button = ui_button_create(view, NULL, fn, NULL);

    if (button) {
        ui_button_set_icon(button, icon);
        ui_button_set_icon_size(button, size);
    }

    return button;
}


static int player_build(ui_view_t* view) {

    if ((player.art = ui_panel_create(view)) == NULL) {
        return -1;
    }

    ui_panel_set_radius(player.art, 12.0);

    if ((player.image = ui_image_create(view, "audio-x-generic")) == NULL) {
        return -1;
    }

    ui_image_set_size(player.image, PLAYER_ART_ICON);


    if ((player.title = ui_label_create(view, PLAYER_NAME)) == NULL) {
        return -1;
    }

    ui_label_set_font(player.title, UI_FONT_BOLD, 17.0);

    if ((player.details = ui_label_create(view, "")) == NULL) {
        return -1;
    }

    if ((player.status = ui_label_create(view, "")) == NULL) {
        return -1;
    }

    ui_label_set_font(player.status, UI_FONT_REGULAR, 12.0);


    if ((player.elapsed = ui_label_create(view, "0:00")) == NULL) {
        return -1;
    }

    ui_label_set_font(player.elapsed, UI_FONT_MEDIUM, 12.0);

    if ((player.seek = ui_slider_create(view, 0.0, 0.0)) == NULL) {
        return -1;
    }

    ui_slider_set_step(player.seek, PLAYER_SEEK_STEP);
    ui_slider_on_change(player.seek, player_on_seek_change, NULL);
    ui_slider_on_commit(player.seek, player_on_seek_commit, NULL);

    if ((player.total = ui_label_create(view, "0:00")) == NULL) {
        return -1;
    }

    ui_label_set_font(player.total, UI_FONT_MEDIUM, 12.0);
    ui_label_set_align(player.total, UI_ALIGN_RIGHT);


    if ((player.back = player_icon_button(view, "media-seek-backward", PLAYER_ICON, player_on_back)) == NULL) {
        return -1;
    }

    if ((player.play = player_icon_button(view, "media-playback-start", PLAYER_ICON_MAIN, player_on_play)) == NULL) {
        return -1;
    }

    if ((player.stop = player_icon_button(view, "media-playback-stop", PLAYER_ICON, player_on_stop)) == NULL) {
        return -1;
    }

    if ((player.forward = player_icon_button(view, "media-seek-forward", PLAYER_ICON, player_on_forward)) == NULL) {
        return -1;
    }

    if ((player.mute = player_icon_button(view, "audio-volume-high", PLAYER_ICON, player_on_mute)) == NULL) {
        return -1;
    }


    if ((player.volume = ui_slider_create(view, 0.0, 100.0)) == NULL) {
        return -1;
    }

    ui_slider_set_step(player.volume, PLAYER_VOLUME_STEP);
    ui_slider_set_value(player.volume, playback.volume);
    ui_slider_on_change(player.volume, player_on_volume, NULL);

    player_restyle(view, NULL);

    return 0;
}


static void player_describe(void) {

    char rate[32];

    snprintf(rate, sizeof(rate), "%.1f", playback.source.rate / 1000.0);

    const size_t length = strlen(rate);

    if (length > 2 && strcmp(rate + length - 2, ".0") == 0) {
        rate[length - 2] = '\0';
    }


    char channels[32];

    if (playback.source.channels == 1) {
        snprintf(channels, sizeof(channels), "Mono");
    } else if (playback.source.channels == 2) {
        snprintf(channels, sizeof(channels), "Stereo");
    } else {
        snprintf(channels, sizeof(channels), "%u channels", playback.source.channels);
    }


    char details[PLAYER_TEXT_MAX];

    snprintf(details, sizeof(details), "%s \xC2\xB7 %s kHz \xC2\xB7 %u-bit \xC2\xB7 %s", playback.source.format == PLAYER_FORMAT_WAV ? "WAV" : "Raw PCM", rate, playback.source.bits, channels);

    ui_label_set_text(player.details, details);
}


static void player_load(const char* path) {

    const char* slash = strrchr(path, '/');

    playback.name = slash && slash[1] ? slash + 1 : path;

    ui_label_set_text(player.title, playback.name);


    char title[PLAYER_TEXT_MAX];

    snprintf(title, sizeof(title), "%.80s - %s", playback.name, PLAYER_NAME);
    ui_window_set_title(player.window, title);


    const char* error = NULL;

    if (player_source_open(&playback.source, path, &error) < 0) {
        playback.error = error;
        ulog_errorf("%s: %s", path, error);
        return;
    }

    playback.loaded = true;

    ui_slider_set_range(player.seek, 0.0, player_seconds(playback.source.frames));

    player_describe();
}


static int player_run(void) {

    ui_event_t event;

    while (!ui_view_closed(player.view)) {

        for (size_t drained = 0; drained < PLAYER_DRAIN_MAX && !ui_view_closed(player.view); drained++) {

            const int pending = ui_next_event(player.conn, &event, 0);

            if (pending < 0) {
                return -1;
            }

            if (pending == 0) {
                break;
            }

            ui_view_dispatch(player.view, &event);
        }

        if (ui_view_closed(player.view)) {
            break;
        }


        player_service();


        int timeout = ui_view_timeout(player.view);

        if (playback.state == PLAYER_PLAYING && (timeout < 0 || timeout > PLAYER_FEED_MS)) {
            timeout = PLAYER_FEED_MS;
        }

        if (ui_view_present(player.view) < 0) {
            return -1;
        }


        struct pollfd fds[2] = {

            {.fd = ui_connection_fd(player.conn), .events = POLLIN,  .revents = 0},
            {.fd = playback.output.fd,            .events = POLLOUT, .revents = 0},
        };

        const nfds_t count = playback.state == PLAYER_PLAYING && playback.output.fd >= 0 && player_output_wants(&playback.output) ? 2 : 1;

        if (poll(fds, count, timeout) < 0 && errno != EINTR) {
            return -1;
        }

        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            return -1;
        }
    }

    return 0;
}


static void show_usage(void) {

    printf("Use: aplus-player [FILE]\n"
           "Play a WAV file or a raw PCM file.\n\n"
           "Raw .pcm files are read as signed 16-bit little-endian stereo at %u Hz.\n",
           PLAYER_PCM_RATE);
}


int main(int argc, char** argv) {

    if (argc > 2 || (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0))) {
        show_usage();
        return argc > 2 ? 1 : 0;
    }

    setvbuf(stdout, NULL, _IONBF, 0);


    if ((player.conn = ui_connect(NULL, 5000)) == NULL) {
        ulog_errorf("ui_connect() failed: %s", strerror(errno));
        return 1;
    }

    if ((player.window = ui_window_create(player.conn, PLAYER_WINDOW_WIDTH, PLAYER_WINDOW_HEIGHT, PLAYER_NAME)) == NULL) {
        ulog_errorf("ui_window_create() failed: %s", strerror(errno));
        ui_disconnect(player.conn);
        return 1;
    }

    if ((player.view = ui_view_create(player.window)) == NULL) {
        ulog_errorf("ui_view_create() failed: %s", strerror(errno));
        ui_window_destroy(player.window);
        ui_disconnect(player.conn);
        return 1;
    }

    if (player_build(player.view) < 0) {
        ulog_errorf("cannot create the widgets");
        ui_view_destroy(player.view);
        ui_window_destroy(player.window);
        ui_disconnect(player.conn);
        return 1;
    }

    ui_view_on_key(player.view, player_on_key, NULL);
    ui_view_on_layout(player.view, player_layout, NULL);
    ui_view_on_theme(player.view, player_restyle, NULL);


    if (argc == 2) {
        player_load(argv[1]);
    }

    player_refresh();
    player_play();


    const int status = player_run();

    if (status < 0) {
        ulog_errorf("the event loop failed: %s", strerror(errno));
    }

    player_output_close(&playback.output);
    player_source_close(&playback.source);

    ui_view_destroy(player.view);
    ui_window_destroy(player.window);
    ui_disconnect(player.conn);

    return status < 0 ? 1 : 0;
}
