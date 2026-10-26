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
 * @brief The control socket: requests that change how the server works, each answered with one UI_CTL_REPLY.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <wm.h>


/**
 * @brief Switches the theme, for a UI_CTL_SET_THEME.
 *
 * @param payload The request body.
 * @param size The length of the body.
 * @param reply Receives the answer.
 */
static void wm_control_set_theme(const uint8_t* payload, size_t size, ui_msg_ctl_reply_t* reply) {

    ui_msg_ctl_theme_t msg;

    if (size != sizeof(msg)) {
        reply->status = EINVAL;
        snprintf(reply->text, sizeof(reply->text), "malformed request");
        return;
    }

    memcpy(&msg, payload, sizeof(msg));

    msg.name[sizeof(msg.name) - 1] = '\0';

    if (wm_theme_switch(msg.name, reply->text, sizeof(reply->text)) < 0) {
        reply->status = errno ? errno : EIO;
    }
}


/**
 * @brief Answers one request from a control peer.
 *
 * @param client The peer.
 * @param type One of UI_CTL_*.
 * @param payload The request body.
 * @param size The length of the body.
 * @return 0, or -1 when the answer cannot be queued.
 */
int wm_control_handle(wm_client_t* client, uint16_t type, const uint8_t* payload, size_t size) {

    ui_msg_ctl_reply_t reply;

    memset(&reply, 0, sizeof(reply));

    switch (type) {

        case UI_CTL_SET_THEME:
            wm_control_set_theme(payload, size, &reply);
            break;

        case UI_CTL_GET_THEME:
            snprintf(reply.text, sizeof(reply.text), "%s", wm.theme.client.name);
            break;

        case UI_CTL_RELOAD:

            if (wm_theme_reload(reply.text, sizeof(reply.text)) < 0) {
                reply.status = errno ? errno : EIO;
            }

            break;

        default:
            reply.status = ENOSYS;
            snprintf(reply.text, sizeof(reply.text), "unknown request %#x", type);
            break;
    }

    return wm_client_queue(client, UI_CTL_REPLY, &reply, sizeof(reply));
}
