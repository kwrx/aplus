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
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <aplus/ui-control.h>


int ui_control_request(const char* path, uint16_t type, const void* payload, size_t size, ui_msg_ctl_reply_t* reply) {

    if (!reply) {
        errno = EINVAL;
        return -1;
    }

    if (!path) {
        path = UI_CONTROL_SOCKET;
    }


    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    if (strlen(path) >= sizeof(addr.sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    strcpy(addr.sun_path, path);


    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0) {
        return -1;
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0 || ui_send_msg(fd, type, payload, size) < 0) {
        close(fd);
        return -1;
    }


    ui_msg_header_t hdr;

    if (ui_recv_all(fd, &hdr, sizeof(hdr)) < 0) {
        close(fd);
        return -1;
    }

    if (hdr.type != UI_CTL_REPLY || hdr.length != sizeof(*reply)) {
        close(fd);
        errno = EPROTO;
        return -1;
    }

    if (ui_recv_all(fd, reply, sizeof(*reply)) < 0) {
        close(fd);
        return -1;
    }

    close(fd);

    reply->text[sizeof(reply->text) - 1] = '\0';

    return 0;
}


int ui_control_set_theme(const char* name, ui_msg_ctl_reply_t* reply) {

    if (!name || strlen(name) >= UI_THEME_NAME_MAX) {
        errno = EINVAL;
        return -1;
    }


    ui_msg_ctl_theme_t msg;
    ui_msg_ctl_reply_t answer;

    memset(&msg, 0, sizeof(msg));
    strcpy(msg.name, name);

    if (ui_control_request(NULL, UI_CTL_SET_THEME, &msg, sizeof(msg), reply ? reply : &answer) < 0) {
        return -1;
    }

    const int32_t status = reply ? reply->status : answer.status;

    if (status) {
        errno = status;
        return -1;
    }

    return 0;
}
