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

#define msg_build(m, t, s) {                                    \
    (m)->msg_header.h_magic = PEACH_MSG_MAGIC;                  \
    (m)->msg_header.h_type = t;                                 \
    (m)->msg_header.h_size = s;                                 \
}

#define msg_send(p, m, s) {                                     \
    if(ipc_msg_send(p, m, sizeof((m)->msg_header) + s) < 0)     \
        die("ipc_msg_send");                                    \
}



#define ack(p) {                                                \
    struct peach_msg ack;                                       \
    msg_build(&ack, PEACH_MSG_ACK, 0);                          \
    msg_send(p, &ack, 0)                                        \
}
