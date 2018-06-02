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


#ifndef _PEACH_H
#define _PEACH_H

#define PEACH_MSG_MAGIC             0x55AA
#define PEACH_MSG_ACK               0x55AA

#define PEACH_MSG_SUBSCRIBE         0x0001


struct peach_msg {
    struct {
        uint16_t h_magic;
        uint16_t h_size;
        uint16_t h_type;
    } msg_header;

    union {
        struct {
            pid_t m_pid;
        } msg_subscribe;

        char msg_data[BUFSIZ];
    };
} __attribute__((packed));

#endif