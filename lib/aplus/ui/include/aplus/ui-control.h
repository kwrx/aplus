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

#ifndef _APLUS_UI_CONTROL_H
#define _APLUS_UI_CONTROL_H

#include <stddef.h>
#include <stdint.h>

#include <aplus/ui.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief The control socket, apart from the window socket so that its permissions can be set on their own.
 */
#define UI_CONTROL_SOCKET "/tmp/aplus-wm-ctl.sock"

/**
 * @brief Where the installed themes live, one <name>.conf each, and the link naming the one in use.
 */
#define UI_THEME_DIR  "/usr/share/themes"
#define UI_THEME_LINK "/etc/ui/theme.conf"

/**
 * @brief The keys the user changed, laid over whichever theme is in use; written by the server alone.
 */
#define UI_SETTINGS_PATH "/etc/ui/settings.conf"


/**
 * @brief The requests a control peer may send, framed like window messages; each gets one UI_CTL_REPLY.
 */

#define UI_CTL_SET_THEME   0x1001
#define UI_CTL_GET_THEME   0x1002
#define UI_CTL_RELOAD      0x1003
#define UI_CTL_SET_OPTIONS 0x1004
#define UI_CTL_GET_OPTION  0x1005

#define UI_CTL_REPLY 0x9001

#define UI_CTL_TEXT_MAX 128

/**
 * @brief How long a theme file's section and key names may be, and how many keys one request may change.
 */
#define UI_CTL_SECTION_MAX 16
#define UI_CTL_KEY_MAX     32
#define UI_CTL_OPTIONS_MAX 8

/**
 * @brief Drops the user's value for a key, giving it back to the theme.
 */
#define UI_CTL_OPTION_RESET (1 << 0)


/**
 * @brief A theme by name, without the directory or the .conf.
 */
typedef struct {

    char name[UI_THEME_NAME_MAX];

} __attribute__((packed)) ui_msg_ctl_theme_t;


/**
 * @brief The answer to a request: 0 or an errno value, and a line of text explaining it.
 */
typedef struct {

    int32_t status;
    char text[UI_CTL_TEXT_MAX];

} __attribute__((packed)) ui_msg_ctl_reply_t;


/**
 * @brief One theme key, as a theme file spells it under its section; a UI_CTL_SET_OPTIONS carries several back to back.
 */
typedef struct {

    char section[UI_CTL_SECTION_MAX];
    char key[UI_CTL_KEY_MAX];
    char value[UI_THEME_PATH_MAX];

    uint32_t flags;

} __attribute__((packed)) ui_msg_ctl_option_t;


/**
 * @brief Sends one request to the control socket and waits for the answer.
 *
 * @param path The socket, or NULL for UI_CONTROL_SOCKET.
 * @param type One of UI_CTL_*.
 * @param payload The request body, or NULL.
 * @param size The length of the body.
 * @param reply Receives the answer, whatever its status.
 * @return 0 when the server answered, or -1 with errno set when it could not be asked.
 */
int ui_control_request(const char* path, uint16_t type, const void* payload, size_t size, ui_msg_ctl_reply_t* reply);

/**
 * @brief Switches the server and every client to an installed theme, and keeps it for the next boot.
 *
 * @param name A theme in UI_THEME_DIR, without the .conf.
 * @param reply Receives the answer, which may carry a warning even on success, or NULL.
 * @return 0, or -1 with errno set: ENOENT for a theme that is not installed, EINVAL for one that does not parse.
 */
int ui_control_set_theme(const char* name, ui_msg_ctl_reply_t* reply);

/**
 * @brief Changes theme keys for every theme, all at once, and keeps them for the next boot.
 *
 * @param options The keys, each with its value or UI_CTL_OPTION_RESET.
 * @param count How many there are, 1 to UI_CTL_OPTIONS_MAX.
 * @param reply Receives the answer, or NULL.
 * @return 0, or -1 with errno set: EINVAL for an unknown key or a value it does not take, which changes nothing.
 */
int ui_control_set_options(const ui_msg_ctl_option_t* options, size_t count, ui_msg_ctl_reply_t* reply);

/**
 * @brief Reads the value a theme key has right now, the user's when there is one and the theme's otherwise.
 *
 * @param section The section, "ui" or "wm".
 * @param key The key.
 * @param out Receives the value as a theme file would spell it.
 * @param size The size of @p out.
 * @return 0, or -1 with errno set: EINVAL for an unknown key.
 */
int ui_control_get_option(const char* section, const char* key, char* out, size_t size);


#ifdef __cplusplus
}
#endif

#endif
