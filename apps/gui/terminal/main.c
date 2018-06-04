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
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <aplus/base.h>
#include <aplus/msg.h>
#include <aplus/peach.h>


int ack = 0;

static void die(char* s) {
    perror(s);
    exit(1);
}

static void peach_handler(int sig) {
    ack = 1;
    printf("ACK\n");
    signal(sig, peach_handler);
}


int main(int argc, char** argv) {
    int fd = open("/etc/peach", O_RDWR);
    if(fd < 0)
        die("open");


    #define wait_ack()  \
        ack = 0; while(!ack) pause();

    struct peach_msg msg;
    msg.msg_header.h_magic = PEACH_MSG_MAGIC;
    msg.msg_header.h_type = PEACH_MSG_SUBSCRIBE;
    msg.msg_header.h_pid = getpid();
    msg.msg_header.h_size = 0;

    
    signal(SIGMSG, peach_handler);
    write(fd, &msg, sizeof(msg.msg_header));
    wait_ack();

    msg.msg_header.h_type = PEACH_MSG_WINDOW_CREATE;
    msg.msg_header.h_size = sizeof(msg.msg_window);
    msg.msg_window.w_width = 300;
    msg.msg_window.w_height = 300;
    write(fd, &msg, sizeof(msg.msg_header) + sizeof(msg.msg_window));
    wait_ack();

    memset(msg.msg_window.w_frame, 0xFF, 300 * 300 * 4);

    int id = msg.msg_window.w_id;

    msg.msg_header.h_type = PEACH_MSG_WINDOW_SET_FLAGS;
    msg.msg_header.h_size = sizeof(msg.msg_window_flags);
    msg.msg_window_flags.w_id = id;
    msg.msg_window_flags.w_flags = PEACH_WINDOW_SHOW | PEACH_WINDOW_BORDERLESS;
    msg.msg_window_flags.w_set = 1;
    write(fd, &msg, sizeof(msg.msg_header) + sizeof(msg.msg_window_flags));
    wait_ack();


   
   
    return 0;
}