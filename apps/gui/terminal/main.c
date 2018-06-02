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


static void die(char* s) {
    perror(s);
    exit(1);
}

static void peach_handler(int sig) {
    printf("ACK!!\n");
    signal(sig, peach_handler);
}


int main(int argc, char** argv) {
    int fd = open("/etc/peach", O_RDWR);
    if(fd < 0)
        die("open");

    struct peach_msg msg;
    msg.msg_header.h_magic = PEACH_MSG_MAGIC;
    msg.msg_header.h_type = PEACH_MSG_SUBSCRIBE;
    msg.msg_header.h_size = sizeof(msg.msg_subscribe);
    msg.msg_subscribe.m_pid = getpid();
    
    signal(SIGMSG, peach_handler);
    write(fd, &msg, sizeof(msg.msg_header) + msg.msg_header.h_size);
    close(fd);

    for(;;)
        pause();

    return 0;
}