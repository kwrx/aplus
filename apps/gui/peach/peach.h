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


#pragma once

#include <aplus/base.h>
#include <aplus/msg.h>
#include <aplus/peach.h>


#define NR_DISPLAY          16


#define msg_build(m, t, s) {                                    \
    (m)->msg_header.h_magic = PEACH_MSG_MAGIC;                  \
    (m)->msg_header.h_type = t;                                 \
    (m)->msg_header.h_size = s;                                 \
}

#define msg_send(p, m, s) {                                     \
    if(ipc_msg_send(p, m, sizeof((m)->msg_header) + s) < 0)     \
        die("ipc_msg_send");                                    \
}



#define reply(m, type, size) {                                  \
    (m)->msg_header.h_magic = PEACH_MSG_MAGIC;                  \
    (m)->msg_header.h_type = type;                              \
    (m)->msg_header.h_size = size;                              \
    msg_send((m)->msg_header.h_pid, m, size)                    \
}

#define reply_error(m, e, d) {                                  \
    (m)->msg_error.e_errno = e;                                 \
    memcpy(&(m)->msg_error.e_details[0], d, strlen(d) + 1);     \
    reply(m, PEACH_MSG_ERROR, strlen(d) + 1 + sizeof(int));     \
}


typedef struct peach_display {
    uint8_t d_active;
    uint16_t d_width;
    uint16_t d_height;
    uint16_t d_bpp;
    uint32_t d_stride;
    void* d_backbuffer;
    void* d_framebuffer;
    
    void (*d_flip) (struct peach_display*);
} peach_display_t;


extern peach_display_t display[];



void die(char*);

void init_display(uint16_t w, uint16_t h, uint16_t bpp, void* framebuffer, int doublebuffer);