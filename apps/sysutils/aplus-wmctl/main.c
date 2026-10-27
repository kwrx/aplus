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
 * @brief Asks the display server to change how it works, over its control socket.
 */

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aplus/ui-control.h>


/**
 * @brief What the messages call this program.
 */
#define WMCTL_NAME "aplus-wmctl"


/**
 * @brief Prints the help and exits.
 *
 * @param status The exit status.
 */
static void show_usage(int status) {

    printf("Use: " WMCTL_NAME " [options] command [argument]\n"
           "Ask the display server to change how it works.\n\n"
           "Commands:\n"
           "   set-theme NAME              switch to " UI_THEME_DIR "/NAME.conf and keep it\n"
           "   get-theme                   show the theme in use\n"
           "   list-themes                 show the installed themes, starring the one in use\n"
           "   reload                      read the theme in use again\n"
           "   get SECTION.KEY             show the value a theme key has right now\n"
           "   set SECTION.KEY VALUE       override a theme key for every theme, and keep it\n"
           "   unset SECTION.KEY           give a key back to the theme\n\n"
           "Options:\n"
           "   -s, --socket                control socket (default: " UI_CONTROL_SOCKET ")\n"
           "       --help                  show this help\n");

    exit(status);
}


/**
 * @brief Sends one request and reports a failure to reach the server, or its refusal, on stderr.
 *
 * @param path The control socket, or NULL for the default.
 * @param type One of UI_CTL_*.
 * @param payload The request body, or NULL.
 * @param size The length of the body.
 * @param reply Receives the answer.
 * @return 0 when the server did what was asked, or -1.
 */
static int wmctl_request(const char* path, uint16_t type, const void* payload, size_t size, ui_msg_ctl_reply_t* reply) {

    if (ui_control_request(path, type, payload, size, reply) < 0) {
        fprintf(stderr, WMCTL_NAME ": cannot reach the display server on %s: %s\n", path ? path : UI_CONTROL_SOCKET, strerror(errno));
        return -1;
    }

    if (reply->status) {
        fprintf(stderr, WMCTL_NAME ": %s\n", reply->text[0] ? reply->text : strerror(reply->status));
        return -1;
    }

    return 0;
}


/**
 * @brief Switches to an installed theme.
 *
 * @param path The control socket, or NULL for the default.
 * @param name The theme.
 * @return The exit status.
 */
static int wmctl_set_theme(const char* path, const char* name) {

    ui_msg_ctl_theme_t msg;
    ui_msg_ctl_reply_t reply;

    memset(&msg, 0, sizeof(msg));

    if (strlen(name) >= sizeof(msg.name)) {
        fprintf(stderr, WMCTL_NAME ": theme name is too long: %s\n", name);
        return 1;
    }

    strcpy(msg.name, name);

    if (wmctl_request(path, UI_CTL_SET_THEME, &msg, sizeof(msg), &reply) < 0) {
        return 1;
    }

    printf("%s\n", reply.text);

    return 0;
}


/**
 * @brief Sends a request that takes no argument and prints the answer.
 *
 * @param path The control socket, or NULL for the default.
 * @param type One of UI_CTL_*.
 * @return The exit status.
 */
static int wmctl_simple(const char* path, uint16_t type) {

    ui_msg_ctl_reply_t reply;

    if (wmctl_request(path, type, NULL, 0, &reply) < 0) {
        return 1;
    }

    printf("%s\n", reply.text);

    return 0;
}


/**
 * @brief Splits a SECTION.KEY argument into the option it names.
 *
 * @param name The argument.
 * @param option Receives the section and the key, everything else zeroed.
 * @return 0, or -1 after reporting a malformed name.
 */
static int wmctl_option(const char* name, ui_msg_ctl_option_t* option) {

    memset(option, 0, sizeof(*option));

    const char* dot = strchr(name, '.');

    if (!dot || dot == name || !dot[1] || (size_t)(dot - name) >= sizeof(option->section) || strlen(dot + 1) >= sizeof(option->key)) {
        fprintf(stderr, WMCTL_NAME ": expected SECTION.KEY, such as ui.font_size: %s\n", name);
        return -1;
    }

    memcpy(option->section, name, (size_t)(dot - name));
    strcpy(option->key, dot + 1);

    return 0;
}


/**
 * @brief Overrides a theme key, or gives it back to the theme.
 *
 * @param path The control socket, or NULL for the default.
 * @param name The key, as SECTION.KEY.
 * @param value The value, or NULL to unset it.
 * @return The exit status.
 */
static int wmctl_set_option(const char* path, const char* name, const char* value) {

    ui_msg_ctl_option_t option;
    ui_msg_ctl_reply_t reply;

    if (wmctl_option(name, &option) < 0) {
        return 1;
    }

    if (value && strlen(value) >= sizeof(option.value)) {
        fprintf(stderr, WMCTL_NAME ": value is too long: %s\n", value);
        return 1;
    }

    if (value) {
        strcpy(option.value, value);
    } else {
        option.flags = UI_CTL_OPTION_RESET;
    }

    if (wmctl_request(path, UI_CTL_SET_OPTIONS, &option, sizeof(option), &reply) < 0) {
        return 1;
    }

    printf("%s\n", reply.text);

    return 0;
}


/**
 * @brief Prints the value a theme key has right now.
 *
 * @param path The control socket, or NULL for the default.
 * @param name The key, as SECTION.KEY.
 * @return The exit status.
 */
static int wmctl_get_option(const char* path, const char* name) {

    ui_msg_ctl_option_t option;
    ui_msg_ctl_reply_t reply;

    if (wmctl_option(name, &option) < 0) {
        return 1;
    }

    if (wmctl_request(path, UI_CTL_GET_OPTION, &option, sizeof(option), &reply) < 0) {
        return 1;
    }

    printf("%s\n", reply.text);

    return 0;
}


/**
 * @brief Lists the themes installed in UI_THEME_DIR, starring the one in use when the server says which.
 *
 * @param path The control socket, or NULL for the default.
 * @return The exit status.
 */
static int wmctl_list_themes(const char* path) {

    ui_msg_ctl_reply_t reply;

    const char* current = "";

    if (ui_control_request(path, UI_CTL_GET_THEME, NULL, 0, &reply) == 0 && !reply.status) {
        current = reply.text;
    }


    DIR* dir = opendir(UI_THEME_DIR);

    if (!dir) {
        fprintf(stderr, WMCTL_NAME ": cannot open %s: %s\n", UI_THEME_DIR, strerror(errno));
        return 1;
    }


    struct dirent* ent;

    while ((ent = readdir(dir))) {

        const size_t length = strlen(ent->d_name);

        if (ent->d_name[0] == '.' || length <= 5 || strcmp(ent->d_name + length - 5, ".conf") != 0) {
            continue;
        }

        const size_t stem = length - 5;
        const bool used   = strlen(current) == stem && strncmp(current, ent->d_name, stem) == 0;

        printf("%c %.*s\n", used ? '*' : ' ', (int)stem, ent->d_name);
    }

    closedir(dir);

    return 0;
}


int main(int argc, char** argv) {

    static struct option long_options[] = {
        {"socket", required_argument, NULL, 's'},
        {"help",   no_argument,       NULL, 'h'},
        {NULL,     0,                 NULL, 0  }
    };

    const char* path = NULL;


    int c, idx;

    while ((c = getopt_long(argc, argv, "s:h", long_options, &idx)) != -1) {

        switch (c) {

            case 's':
                path = optarg;
                break;
            case 'h':
                show_usage(0);
                break;
            default:
                show_usage(1);
                break;
        }
    }


    if (optind >= argc) {
        show_usage(1);
    }

    const char* command = argv[optind];
    const int extra     = argc - optind - 1;

    if (strcmp(command, "set-theme") == 0 && extra == 1) {
        return wmctl_set_theme(path, argv[optind + 1]);
    }

    if (strcmp(command, "get-theme") == 0 && extra == 0) {
        return wmctl_simple(path, UI_CTL_GET_THEME);
    }

    if (strcmp(command, "reload") == 0 && extra == 0) {
        return wmctl_simple(path, UI_CTL_RELOAD);
    }

    if (strcmp(command, "list-themes") == 0 && extra == 0) {
        return wmctl_list_themes(path);
    }

    if (strcmp(command, "get") == 0 && extra == 1) {
        return wmctl_get_option(path, argv[optind + 1]);
    }

    if (strcmp(command, "set") == 0 && extra == 2) {
        return wmctl_set_option(path, argv[optind + 1], argv[optind + 2]);
    }

    if (strcmp(command, "unset") == 0 && extra == 1) {
        return wmctl_set_option(path, argv[optind + 1], NULL);
    }

    show_usage(1);

    return 1;
}
