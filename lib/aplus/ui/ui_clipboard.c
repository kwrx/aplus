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
#include <stdlib.h>
#include <string.h>

#include <aplus/ui.h>

#include "ui_internal.h"


int ui_clipboard_set(ui_connection_t* conn, const char* mime, const void* data, size_t size) {

    if (!conn || (size && !data)) {
        errno = EINVAL;
        return -1;
    }

    if (!mime) {
        mime = size ? UI_CLIPBOARD_TEXT : "";
    }

    if (strlen(mime) >= UI_CLIPBOARD_MIME_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (size > UI_CLIPBOARD_MAX) {
        errno = EMSGSIZE;
        return -1;
    }


    const size_t length = sizeof(ui_msg_clipboard_t) + size;

    uint8_t* msg = (uint8_t*)calloc(1, length);

    if (!msg) {
        return -1;
    }

    ui_msg_clipboard_t* clip = (ui_msg_clipboard_t*)msg;

    strncpy(clip->mime, mime, UI_CLIPBOARD_MIME_MAX - 1);

    if (size) {
        memcpy(msg + sizeof(*clip), data, size);
    }

    int e = ui_send_msg(conn->fd, UI_REQ_CLIPBOARD_SET, msg, length);

    free(msg);

    return e;
}


int ui_clipboard_set_text(ui_connection_t* conn, const char* text) {
    return ui_clipboard_set(conn, UI_CLIPBOARD_TEXT, text, text ? strlen(text) : 0);
}


int ui_clipboard_request(ui_connection_t* conn, uint32_t* serial) {

    if (!conn) {
        errno = EINVAL;
        return -1;
    }

    ui_msg_clipboard_get_t req = {.serial = ++conn->clipboard.serial};

    if (ui_send_msg(conn->fd, UI_REQ_CLIPBOARD_GET, &req, sizeof(req)) < 0) {
        return -1;
    }

    if (serial) {
        *serial = req.serial;
    }

    return 0;
}


const void* ui_clipboard_data(ui_connection_t* conn, size_t* size, const char** mime) {

    if (!conn || !conn->clipboard.data) {

        if (size) {
            *size = 0;
        }

        if (mime) {
            *mime = "";
        }

        return NULL;
    }

    if (size) {
        *size = conn->clipboard.size;
    }

    if (mime) {
        *mime = conn->clipboard.mime;
    }

    return conn->clipboard.data;
}
