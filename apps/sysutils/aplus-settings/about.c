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

/**
 * @brief The About page: the system, its hardware and uptime, and the device's name, which can be changed.
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/statfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <aplus/fb.h>
#include <aplus/ui-draw.h>

#include "settings.h"


#define ABOUT_REFRESH_MS  1000
#define ABOUT_LOGO_SIZE   48
#define ABOUT_LOGO_ICON   "computer"
#define ABOUT_FRAMEBUFFER "/dev/fb0"
#define ABOUT_CPUINFO     "/proc/cpuinfo"
#define ABOUT_HOSTNAME    "/etc/hostname"
#define ABOUT_NAME_MAX    63


/**
 * @brief The rows of the card.
 */
typedef enum {

    ABOUT_HOSTNAME_ROW = 0,
    ABOUT_PROCESSOR,
    ABOUT_ARCHITECTURE,
    ABOUT_MEMORY,
    ABOUT_DISK,
    ABOUT_DISPLAY,
    ABOUT_PROCESSES,
    ABOUT_UPTIME,

    ABOUT_COUNT,

} about_row_t;


/**
 * @brief The names the rows are shown under.
 */
static const char* const about_keys[ABOUT_COUNT] = {
    "Device name", "Processor", "Architecture", "Memory", "Disk", "Display", "Processes", "Uptime",
};


/**
 * @brief The page's widgets, where the logo goes, and when the card is next read again.
 */
static struct {

    ui_widget_t* title;
    ui_widget_t* subtitle;
    ui_widget_t* hostname;
    ui_widget_t* rename;
    ui_widget_t* status;

    settings_card_t card;

    ui_rect_t logo;

    uint64_t next_refresh;

} about;


/**
 * @brief Reports the plural suffix for a count.
 *
 * @param count The count.
 * @return "" for one, "s" otherwise.
 */
static const char* about_plural(unsigned long count) {
    return count == 1 ? "" : "s";
}


/**
 * @brief Copies text, folding every run of whitespace into one space and trimming both ends.
 *
 * @param text The text.
 * @param out Receives the result.
 * @param size The size of @p out.
 */
static void about_squeeze(const char* text, char* out, size_t size) {

    size_t length = 0;
    bool space    = false;

    for (; *text && length + 1 < size; text++) {

        if (isspace((unsigned char)*text)) {
            space = length > 0;
            continue;
        }

        if (space && length + 2 < size) {
            out[length++] = ' ';
        }

        space         = false;
        out[length++] = *text;
    }

    out[length] = '\0';
}


/**
 * @brief Formats how much of a total is used, as "used of total used".
 *
 * @param used The bytes in use.
 * @param total The bytes there are.
 * @param out Receives the text.
 * @param size The size of @p out.
 */
static void about_format_usage(uint64_t used, uint64_t total, char* out, size_t size) {

    char a[32];
    char b[32];

    settings_format_size(used, a, sizeof(a));
    settings_format_size(total, b, sizeof(b));

    snprintf(out, size, "%s of %s used", a, b);
}


/**
 * @brief Formats an uptime in its two largest units.
 *
 * @param seconds The uptime.
 * @param out Receives the text.
 * @param size The size of @p out.
 */
static void about_format_uptime(unsigned long seconds, char* out, size_t size) {

    const unsigned long days    = seconds / 86400;
    const unsigned long hours   = seconds / 3600 % 24;
    const unsigned long minutes = seconds / 60 % 60;

    if (days) {
        snprintf(out, size, "%lu day%s, %lu hour%s", days, about_plural(days), hours, about_plural(hours));
    } else if (hours) {
        snprintf(out, size, "%lu hour%s, %lu minute%s", hours, about_plural(hours), minutes, about_plural(minutes));
    } else if (minutes) {
        snprintf(out, size, "%lu minute%s", minutes, about_plural(minutes));
    } else {
        snprintf(out, size, "Less than a minute");
    }
}


/**
 * @brief Shows a line under the card, or clears it.
 *
 * @param text The line, or "" for none.
 */
static void about_report(const char* text) {
    settings_fit(about.status, text);
}


/**
 * @brief Checks a device name: letters, digits and hyphens in dot-separated labels, none starting or ending with a hyphen.
 *
 * @param name The name.
 * @return true when it is one.
 */
static bool about_valid_name(const char* name) {

    const size_t length = strlen(name);

    if (length == 0 || length > ABOUT_NAME_MAX) {
        return false;
    }

    for (size_t i = 0; i < length; i++) {

        const char c    = name[i];
        const bool edge = i == 0 || i == length - 1 || name[i - 1] == '.' || name[i + 1] == '.';

        if (c == '.' && (i == 0 || i == length - 1 || name[i - 1] == '.')) {
            return false;
        }

        if (c == '-' && edge) {
            return false;
        }

        if (!isalnum((unsigned char)c) && c != '-' && c != '.') {
            return false;
        }
    }

    return true;
}


/**
 * @brief Renames the device to what the field says, now and for the next boot.
 *
 * @param widget The Rename button or the field.
 * @param user Unused.
 */
static void about_on_rename(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    char text[SETTINGS_TEXT_MAX];
    char name[ABOUT_NAME_MAX + 2];

    snprintf(name, sizeof(name), "%s", ui_entry_text(about.hostname));

    if (!about_valid_name(name)) {
        about_report("Use letters, digits and hyphens, up to 63 characters.");
        return;
    }

    if (sethostname(name, strlen(name)) < 0) {
        snprintf(text, sizeof(text), "Cannot rename the device: %s", strerror(errno));
        about_report(text);
        return;
    }

    settings_row_set(&about.card.rows[ABOUT_HOSTNAME_ROW], name);

    snprintf(text, sizeof(text), "%s\n", name);

    if (settings_write_file(ABOUT_HOSTNAME, text) < 0) {
        snprintf(text, sizeof(text), "Renamed, but the next boot will not keep it: %s", strerror(errno));
        about_report(text);
        return;
    }

    snprintf(text, sizeof(text), "Renamed to %s.", name);
    about_report(text);
}


/**
 * @brief Reads what does not change while the system runs: the version, processor, architecture and display.
 */
static void about_probe(void) {

    char text[SETTINGS_TEXT_MAX];

    struct utsname uts;

    if (uname(&uts) == 0) {

        settings_row_set(&about.card.rows[ABOUT_ARCHITECTURE], uts.machine);

        ui_label_set_text(about.title, uts.sysname);

        snprintf(text, sizeof(text), "Version %.48s (%.48s)", uts.release, uts.version);
        ui_label_set_text(about.subtitle, text);
    }


    FILE* fp = fopen(ABOUT_CPUINFO, "r");

    if (fp) {

        char line[256];
        char model[96] = "";
        int count      = 0;

        while (fgets(line, sizeof(line), fp)) {

            const char* colon = strchr(line, ':');

            if (!colon) {
                continue;
            }

            if (strncmp(line, "processor", 9) == 0) {
                count++;
            } else if (!model[0] && strncmp(line, "model name", 10) == 0) {
                about_squeeze(colon + 1, model, sizeof(model));
            }
        }

        fclose(fp);

        if (model[0] && count > 1) {
            snprintf(text, sizeof(text), "%s " SETTINGS_TIMES " %d", model, count);
            settings_row_set(&about.card.rows[ABOUT_PROCESSOR], text);
        } else if (model[0]) {
            settings_row_set(&about.card.rows[ABOUT_PROCESSOR], model);
        }
    }


    const int fd = open(ABOUT_FRAMEBUFFER, O_RDONLY);

    if (fd >= 0) {

        struct fb_var_screeninfo var;
        struct fb_fix_screeninfo fix;

        memset(&fix, 0, sizeof(fix));

        if (ioctl(fd, FBIOGET_VSCREENINFO, &var) == 0) {

            char id[sizeof(fix.id) + 1] = "";

            if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) == 0) {
                memcpy(id, fix.id, sizeof(fix.id));
                id[sizeof(fix.id)] = '\0';
            }

            if (id[0]) {
                snprintf(text, sizeof(text), "%u " SETTINGS_TIMES " %u (%s)", var.xres, var.yres, id);
            } else {
                snprintf(text, sizeof(text), "%u " SETTINGS_TIMES " %u", var.xres, var.yres);
            }

            settings_row_set(&about.card.rows[ABOUT_DISPLAY], text);
        }

        close(fd);
    }
}


/**
 * @brief Reads what changes while the system runs: memory, disk, processes and uptime.
 */
static void about_refresh(void) {

    char text[SETTINGS_TEXT_MAX];

    struct sysinfo si;

    if (sysinfo(&si) == 0) {

        const uint64_t unit  = si.mem_unit ? si.mem_unit : 1;
        const uint64_t total = (uint64_t)si.totalram * unit;
        const uint64_t free  = (uint64_t)si.freeram * unit;

        about_format_usage(total > free ? total - free : 0, total, text, sizeof(text));
        settings_row_set(&about.card.rows[ABOUT_MEMORY], text);

        snprintf(text, sizeof(text), "%u", (unsigned)si.procs);
        settings_row_set(&about.card.rows[ABOUT_PROCESSES], text);

        about_format_uptime((unsigned long)si.uptime, text, sizeof(text));
        settings_row_set(&about.card.rows[ABOUT_UPTIME], text);
    }


    struct statfs fs;

    if (statfs("/", &fs) == 0 && fs.f_blocks > 0) {

        const uint64_t block = (uint64_t)fs.f_bsize;
        const uint64_t total = (uint64_t)fs.f_blocks * block;
        const uint64_t free  = (uint64_t)fs.f_bfree * block;

        about_format_usage(total > free ? total - free : 0, total, text, sizeof(text));
        settings_row_set(&about.card.rows[ABOUT_DISK], text);
    }
}


/**
 * @brief Creates the page's widgets.
 *
 * @return 0 on success, -1 otherwise.
 */
static int about_build(void) {

    about.title    = settings_own(ui_label_create(settings.view, "aplus"));
    about.subtitle = settings_own(ui_label_create(settings.view, ""));

    if (!about.title || !about.subtitle || settings_card_build(&about.card, about_keys, ABOUT_COUNT, 1U << ABOUT_HOSTNAME_ROW) < 0) {
        return -1;
    }

    about.hostname = settings_own(ui_entry_create(settings.view, "Device name"));
    about.rename   = settings_own(ui_button_create(settings.view, "Rename", about_on_rename, NULL));
    about.status   = settings_own(ui_label_create(settings.view, ""));

    if (!about.hostname || !about.rename || !about.status) {
        return -1;
    }

    ui_entry_on_submit(about.hostname, about_on_rename, NULL);

    about_probe();
    about_refresh();

    return 0;
}


/**
 * @brief Lays out the logo beside the system's name and version, the card below, and the device name's field in its row.
 *
 * @param page The page's rectangle.
 */
static void about_layout(ui_rect_t page) {

    const int line  = settings_line_height();
    const int title = (int)(settings_title_size() * 1.4 + 0.5);
    const int text  = page.x + ABOUT_LOGO_SIZE + 2 * SETTINGS_GAP;
    const int top   = page.y + (ABOUT_LOGO_SIZE - title - line) / 2;

    about.logo = (ui_rect_t){page.x, page.y, ABOUT_LOGO_SIZE, ABOUT_LOGO_SIZE};

    ui_widget_set_rect(about.title, text, top, page.x + page.width - text, title);
    ui_widget_set_rect(about.subtitle, text, top + title, page.x + page.width - text, line);


    const int card = page.y + ABOUT_LOGO_SIZE + SETTINGS_SECTION_GAP;
    const int high = settings_card_place(&about.card, page.x, card, page.width);

    const ui_rect_t value = settings_card_value(&about.card, ABOUT_HOSTNAME_ROW);
    const int button      = settings_text_width("Rename", UI_FONT_REGULAR) + 2 * SETTINGS_CARD_PADDING;
    const int field       = value.width - button - SETTINGS_GAP;

    ui_widget_set_rect(about.hostname, value.x, value.y + 5, field > 0 ? field : 0, value.height - 10);
    ui_widget_set_rect(about.rename, value.x + value.width - button, value.y + 5, button, value.height - 10);

    ui_widget_set_rect(about.status, page.x, card + high + SETTINGS_GAP, page.width, line);
}


/**
 * @brief Gives the page the fonts and colours of the theme in use.
 */
static void about_restyle(void) {

    ui_label_set_font(about.title, UI_FONT_BOLD, settings_title_size() * 1.15);
    ui_label_set_color(about.subtitle, ui_theme()->text_muted);
    ui_label_set_color(about.status, ui_theme()->text_muted);

    settings_card_restyle(&about.card);
}


/**
 * @brief Fills the device name's field afresh and asks for the card to be read again straight away.
 */
static void about_enter(void) {

    struct utsname uts;

    if (uname(&uts) == 0) {
        ui_entry_set_text(about.hostname, uts.nodename);
    }

    about_report("");

    about.next_refresh = 0;
}


/**
 * @brief Reads the card again once a second while the page is on show.
 *
 * @param now The clock, in milliseconds.
 * @param visible Whether the page is on show.
 * @return How long until the next read, or -1 while hidden.
 */
static int about_tick(uint64_t now, bool visible) {

    if (!visible) {
        return -1;
    }

    if (now >= about.next_refresh) {
        about_refresh();
        about.next_refresh = now + ABOUT_REFRESH_MS;
    }

    return (int)(about.next_refresh - now);
}


/**
 * @brief Paints the logo beside the system's name, over the background the view laid down.
 *
 * @param cr A context over the window.
 */
static void about_paint(cairo_t* cr) {

    const ui_rect_t rect = about.logo;

    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }

    const ui_color_t background = ui_theme()->background;

    cairo_save(cr);

    cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
    cairo_clip(cr);

    cairo_set_source_rgb(cr, background.r, background.g, background.b);
    cairo_paint(cr);

    ui_draw_icon_named(cr, rect, ABOUT_LOGO_ICON, ABOUT_LOGO_SIZE);

    cairo_restore(cr);

    ui_window_damage(settings.window, rect.x, rect.y, rect.width, rect.height);
}


/**
 * @brief The About page.
 */
const settings_page_t settings_about = {
    .name    = "about",
    .label   = "About",
    .icon    = "computer",
    .build   = about_build,
    .layout  = about_layout,
    .restyle = about_restyle,
    .enter   = about_enter,
    .tick    = about_tick,
    .paint   = about_paint,
};
