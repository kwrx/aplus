/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#include <aplus/base.h>
#include <aplus/fb.h>
#include <aplus/events.h>
#include <aplus/sysconfig.h>
#include <aplus/utils/list.h>
#include <aplus/utils/async.h>

#include <aplus/peach.h>
#include "../peach.h"



API(PEACH_MSG_ERROR) {
    API_ERROR(msg, ENOSYS, "Invalid request type");
}


API(PEACH_MSG_SUBSCRIBE) {
    API_REPLY(msg, PEACH_MSG_SUBSCRIBE, 0);
}



API(PEACH_MSG_DISPLAY) {
    if(msg->msg_display.d_index > NR_DISPLAY)
        API_ERROR(msg, EINVAL, "Index > NR_DISPLAY, invalid display number");


    msg->msg_display.d_active = display[msg->msg_display.d_index].d_active;
    msg->msg_display.d_width = display[msg->msg_display.d_index].d_width;
    msg->msg_display.d_height = display[msg->msg_display.d_index].d_height;
    msg->msg_display.d_bpp = display[msg->msg_display.d_index].d_bpp;
    
    API_REPLY(msg, PEACH_MSG_DISPLAY, sizeof(msg->msg_display));
}




API(PEACH_MSG_WINDOW_CREATE) {
    if(!msg->msg_window.w_width || !msg->msg_window.w_height)
        API_ERROR(msg, EINVAL, "Size cannot be zero");

    peach_window_t* window = window_create(msg->msg_header.h_pid, msg->msg_window.w_width, msg->msg_window.w_height);
    if(!window)
        API_ERROR(msg, errno, "Could not create window");

    msg->msg_window.w_id = window->w_id;
    msg->msg_window.w_width = window->w_width;
    msg->msg_window.w_height = window->w_height;
    msg->msg_window.w_frame = window->w_frame;
    
    API_REPLY(msg, PEACH_MSG_WINDOW_CREATE, sizeof(msg->msg_window));
}




API(PEACH_MSG_WINDOW_DESTROY) {
    if(window_destroy(msg->msg_window.w_id) != 0)
        API_ERROR(msg, errno, "Could not destroy window");

    API_REPLY(msg, PEACH_MSG_WINDOW_DESTROY, 0);
}



API(PEACH_MSG_WINDOW_SET_TITLE) {
    if(window_set_title(
        msg->msg_window_title.w_id,
        msg->msg_window_title.w_title
    ) != 0)
        API_ERROR(msg, errno, "Coult not set title");

    API_REPLY(msg, PEACH_MSG_WINDOW_SET_TITLE, 0);
}



API(PEACH_MSG_WINDOW_GET_TITLE) {
    if(window_get_title (
        msg->msg_window_title.w_id,
        msg->msg_window_title.w_title
    ) != 0)
        API_ERROR(msg, errno, "Coult not set title");

    API_REPLY(msg, PEACH_MSG_WINDOW_GET_TITLE, sizeof(msg->msg_window_title) + strlen(msg->msg_window_title.w_title));
}



API(PEACH_MSG_WINDOW_SET_FLAGS) {
    if(window_set_flags (
        msg->msg_window_flags.w_id, 
        msg->msg_window_flags.w_flags, 
        msg->msg_window_flags.w_set
    ) != 0)
        API_ERROR(msg, errno, "Coult not set flags");

    API_REPLY(msg, PEACH_MSG_WINDOW_SET_FLAGS, 0);
}



API(PEACH_MSG_WINDOW_GET_FLAGS) {
    if(window_get_flags (
        msg->msg_window_flags.w_id, 
        &msg->msg_window_flags.w_flags
    ) != 0)
        API_ERROR(msg, errno, "Coult not set flags");

    API_REPLY(msg, PEACH_MSG_WINDOW_GET_FLAGS, sizeof(msg->msg_window_flags));
}


API(PEACH_MSG_WINDOW_SET_BOUNDS) {
    if(window_set_bounds (
        msg->msg_window_bounds.w_id, 
        msg->msg_window_bounds.w_x,
        msg->msg_window_bounds.w_y,
        msg->msg_window_bounds.w_width,
        msg->msg_window_bounds.w_height
    ) != 0)
        API_ERROR(msg, errno, "Coult not set bounds");

    API_REPLY(msg, PEACH_MSG_WINDOW_SET_BOUNDS, 0);
}



API(PEACH_MSG_WINDOW_GET_BOUNDS) {
    if(window_get_bounds (
        msg->msg_window_bounds.w_id, 
        &msg->msg_window_bounds.w_x,
        &msg->msg_window_bounds.w_y,
        &msg->msg_window_bounds.w_width,
        &msg->msg_window_bounds.w_height
    ) != 0)
        API_ERROR(msg, errno, "Coult not get bounds");

    API_REPLY(msg, PEACH_MSG_WINDOW_GET_BOUNDS, sizeof(msg->msg_window_bounds));
}






api_t api_list[] = {
    API_DECL(PEACH_MSG_ERROR),
    API_DECL(PEACH_MSG_SUBSCRIBE),
    API_DECL(PEACH_MSG_DISPLAY),
    API_DECL(PEACH_MSG_WINDOW_CREATE),
    API_DECL(PEACH_MSG_WINDOW_DESTROY),
    API_DECL(PEACH_MSG_WINDOW_SET_TITLE),
    API_DECL(PEACH_MSG_WINDOW_GET_TITLE),
    API_DECL(PEACH_MSG_WINDOW_SET_BOUNDS),
    API_DECL(PEACH_MSG_WINDOW_GET_BOUNDS),
    API_DECL(PEACH_MSG_WINDOW_SET_FLAGS),
    API_DECL(PEACH_MSG_WINDOW_GET_FLAGS),
};