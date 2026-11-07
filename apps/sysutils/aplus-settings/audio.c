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

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <aplus/soundcard.h>

#include "settings.h"


#define AUDIO_MIXER_PATH "/dev/mixer"
#define AUDIO_REFRESH_MS 1000
#define AUDIO_STEP       1.0


static const char* const audio_labels[SOUND_MIXER_NRDEVICES] = {
    [SOUND_MIXER_VOLUME] = "Master volume", [SOUND_MIXER_BASS] = "Bass",         [SOUND_MIXER_TREBLE] = "Treble",   [SOUND_MIXER_SYNTH] = "Synthesizer", [SOUND_MIXER_PCM] = "Applications",     [SOUND_MIXER_SPEAKER] = "PC speaker",
    [SOUND_MIXER_LINE] = "Line in",         [SOUND_MIXER_MIC] = "Microphone",    [SOUND_MIXER_CD] = "CD",           [SOUND_MIXER_IMIX] = "Monitor mix",  [SOUND_MIXER_ALTPCM] = "Alternate PCM", [SOUND_MIXER_RECLEV] = "Recording level",
    [SOUND_MIXER_IGAIN] = "Input gain",     [SOUND_MIXER_OGAIN] = "Output gain", [SOUND_MIXER_LINE1] = "Line in 1", [SOUND_MIXER_LINE2] = "Line in 2",   [SOUND_MIXER_LINE3] = "Line in 3",      [SOUND_MIXER_PHONEIN] = "Phone",
    [SOUND_MIXER_VIDEO] = "Video",          [SOUND_MIXER_RADIO] = "Radio",       [SOUND_MIXER_MONITOR] = "Monitor",
};


static const char* const audio_none_keys[] = {
    "Sound card",
};


static struct {

    settings_header_t header;
    settings_card_t card;

    const char* keys[SETTINGS_CARD_ROWS];
    int channels[SETTINGS_CARD_ROWS];
    uint32_t stereo;
    size_t count;

    ui_widget_t* sliders[SETTINGS_CARD_ROWS];
    ui_widget_t* levels[SETTINGS_CARD_ROWS];
    ui_widget_t* status;

    uint64_t next_refresh;

} audio;


static void audio_report(const char* text) {
    settings_fit(audio.status, text);
}


static int audio_mixer_open(void) {
    return open(AUDIO_MIXER_PATH, O_RDWR | O_CLOEXEC);
}


static int audio_clamp(int level) {
    return level < 0 ? 0 : level > 100 ? 100 : level;
}


static int audio_level(uint32_t raw, int channel) {

    const int left  = (int)(raw & 0xFF);
    const int right = (int)((raw >> 8) & 0xFF);

    return audio_clamp((audio.stereo & (1U << channel)) ? (left + right) / 2 : left);
}


static void audio_show_level(size_t row, int level) {

    char text[16];

    snprintf(text, sizeof(text), "%d%%", level);
    ui_label_set_text(audio.levels[row], text);
}


static void audio_scan(void) {

    const int fd = audio_mixer_open();

    if (fd < 0) {
        return;
    }

    int devmask    = 0;
    int stereomask = 0;

    if (ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devmask) == 0) {

        if (ioctl(fd, SOUND_MIXER_READ_STEREODEVS, &stereomask) < 0) {
            stereomask = 0;
        }

        audio.stereo = (uint32_t)stereomask;

        for (int i = 0; i < SOUND_MIXER_NRDEVICES && audio.count < SETTINGS_CARD_ROWS; i++) {

            if (!(devmask & (1 << i)) || !audio_labels[i]) {
                continue;
            }

            audio.channels[audio.count] = i;
            audio.keys[audio.count]     = audio_labels[i];
            audio.count++;
        }
    }

    close(fd);
}


static void audio_refresh(void) {

    if (audio.count == 0) {
        return;
    }

    const int fd = audio_mixer_open();

    if (fd < 0) {
        return;
    }

    for (size_t i = 0; i < audio.count; i++) {

        if (ui_slider_dragging(audio.sliders[i])) {
            continue;
        }

        uint32_t raw = 0;

        if (ioctl(fd, SOUND_MIXER_READ(audio.channels[i]), &raw) < 0) {
            continue;
        }

        const int level = audio_level(raw, audio.channels[i]);

        ui_slider_set_value(audio.sliders[i], (double)level);
        audio_show_level(i, level);
    }

    close(fd);
}


static void audio_on_change(ui_widget_t* widget, double value, void* user) {

    (void)widget;

    const size_t row  = (size_t)(intptr_t)user;
    const int channel = audio.channels[row];

    const int level = audio_clamp((int)(value + 0.5));

    uint32_t raw = (uint32_t)level;

    if (audio.stereo & (1U << channel)) {
        raw |= (uint32_t)level << 8;
    }

    const int fd = audio_mixer_open();
    const int e  = fd < 0 || ioctl(fd, SOUND_MIXER_WRITE(channel), &raw) < 0 ? errno : 0;

    if (fd >= 0) {
        close(fd);
    }

    if (e) {

        char text[SETTINGS_TEXT_MAX];

        snprintf(text, sizeof(text), "Cannot change %s: %s", audio.keys[row], strerror(e));
        audio_report(text);

        return;
    }

    audio_show_level(row, audio_level(raw, channel));
    audio_report("");
}


static int audio_build(void) {

    if (settings_header_build(&audio.header, "Audio", "How loud the system plays, and how loud it hears.") < 0) {
        return -1;
    }

    audio_scan();

    if (audio.count == 0) {

        if (settings_card_build(&audio.card, audio_none_keys, 1, 0) < 0) {
            return -1;
        }

        settings_row_set(&audio.card.rows[0], "None found");

    } else {

        if (settings_card_build(&audio.card, audio.keys, audio.count, (1U << audio.count) - 1) < 0) {
            return -1;
        }

        for (size_t i = 0; i < audio.count; i++) {

            audio.sliders[i] = settings_own(ui_slider_create(settings.view, 0.0, 100.0));
            audio.levels[i]  = settings_own(ui_label_create(settings.view, ""));

            if (!audio.sliders[i] || !audio.levels[i]) {
                return -1;
            }

            ui_slider_set_step(audio.sliders[i], AUDIO_STEP);
            ui_slider_on_change(audio.sliders[i], audio_on_change, (void*)(intptr_t)i);
            ui_label_set_align(audio.levels[i], UI_ALIGN_RIGHT);
        }
    }

    if ((audio.status = settings_own(ui_label_create(settings.view, ""))) == NULL) {
        return -1;
    }

    audio_refresh();

    return 0;
}


static void audio_layout(ui_rect_t page) {

    int y = settings_header_place(&audio.header, page, 0);

    y += settings_card_place(&audio.card, page.x, y, page.width);

    const int text = settings_text_width("100%", UI_FONT_REGULAR);

    for (size_t i = 0; i < audio.count; i++) {

        const ui_rect_t value = settings_card_value(&audio.card, i);
        const int slider      = value.width - text - SETTINGS_GAP;

        ui_widget_set_rect(audio.sliders[i], value.x, value.y + 5, slider > 0 ? slider : 0, value.height - 10);
        ui_widget_set_rect(audio.levels[i], value.x + value.width - text, value.y, text, value.height);
    }

    ui_widget_set_rect(audio.status, page.x, y + SETTINGS_GAP, page.width, settings_line_height());
}


static void audio_restyle(void) {

    settings_header_restyle(&audio.header);
    settings_card_restyle(&audio.card);

    for (size_t i = 0; i < audio.count; i++) {
        ui_label_set_color(audio.levels[i], ui_theme()->text);
    }

    ui_label_set_color(audio.status, ui_theme()->text_muted);
}


static void audio_enter(void) {

    audio_report("");

    audio.next_refresh = 0;
}


static int audio_tick(uint64_t now, bool visible) {

    if (!visible || audio.count == 0) {
        return -1;
    }

    if (now >= audio.next_refresh) {
        audio_refresh();
        audio.next_refresh = now + AUDIO_REFRESH_MS;
    }

    return (int)(audio.next_refresh - now);
}


const settings_page_t settings_audio = {
    .name    = "audio",
    .label   = "Audio",
    .icon    = "audio-volume-high",
    .build   = audio_build,
    .layout  = audio_layout,
    .restyle = audio_restyle,
    .enter   = audio_enter,
    .tick    = audio_tick,
};
