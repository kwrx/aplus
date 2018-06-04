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
#include "peach.h"

static void show_usage(int argc, char** argv) {
    printf(
        "Use: peach [OPTIONS...]\n"
        "Desktop Manager Server\n\n"
        "   -k, --kill                  kill all running server\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus sysutils) 0.1\n"
        "Copyright (c) 2016 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}


void die(char* s) {
    perror(s);
    exit(1);
}



int main(int argc, char** argv) {
   
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'q'},
        { NULL, 0, NULL, 0 }
    };


    int c, idx;
    while((c = getopt_long(argc, argv, "k", long_options, &idx)) != -1) {
        switch(c) {
            case 'q':
                show_version(argc, argv);
                break;
            case 'h':
            case '?':
                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }


    int ld = open("/dev/log", O_WRONLY);
    if(ld >= 0) {
        dup2(ld, STDOUT_FILENO);
        dup2(ld, STDERR_FILENO);
    }


    memset(display, 0, sizeof(peach_display_t) * NR_DISPLAY);
    memset(window, 0, sizeof(peach_window_t) * NR_WINDOW);


    if(mkfifo("/etc/peach", 0666) != 0)
        die("peach: server already running");

    int fd = open("/etc/peach", O_RDWR);
    if(fd < 0)
        die("peach: pipe");


    
    int fb = open((const char*) sysconfig("screen.device", "/dev/fb0"), O_RDONLY);
    if(fb < 0)
        die("peach: screen-device");

    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    ioctl(fb, FBIOGET_VSCREENINFO, &var);
    ioctl(fb, FBIOGET_FSCREENINFO, &fix);
    close(fb);

    if(!fix.smem_start)
        die("peach: could not open default display");

    if(init_display(var.xres, var.yres, var.bits_per_pixel, (void*) fix.smem_start, 0) != 0)
        die("peach: display");




    #define pipe_read(fd, buf, len) do {    \
        int e;                              \
        if((e = read(fd, buf, len)) == len) \
            break;                          \
        if(e == -1 && errno == EWOULDBLOCK) \
            continue;                       \
                                            \
        die("peach: read(" #buf ")");       \
                                            \
    } while(1);
    


    do {
        struct peach_msg msg;
        pipe_read(fd, &msg.msg_header.h_magic, sizeof(msg.msg_header.h_magic));

        if(msg.msg_header.h_magic != PEACH_MSG_MAGIC)
            continue;

        pipe_read(fd, &msg.msg_header.h_pid, sizeof(msg.msg_header) - sizeof(msg.msg_header.h_magic));
        pipe_read(fd, &msg.msg_data, msg.msg_header.h_size);

        if(!api_list[msg.msg_header.h_type]) {
            api_list[PEACH_MSG_ERROR] (&msg);
            continue;
        }

        api_list[msg.msg_header.h_type] (&msg);
    } while(1);
    
    return 0;
}