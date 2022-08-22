/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2022 Antonino Natale
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


#include <iostream>
#include <string>
#include <vector>
#include <getopt.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <aplus/fb.h>



class Window {

    public:

        Window(std::string title, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
            : __title(std::move(title))
            , __x(x)
            , __y(y)
            , __w(w)
            , __h(h) {}


        const auto& title() const {
            return __title;
        }

        const auto& x() const {
            return __x;
        }

        const auto& y() const {
            return __y;
        }

        const auto& w() const {
            return __w;
        }

        const auto& h() const {
            return __h;
        }

    private:
        
        std::string __title;
        uint16_t __x;
        uint16_t __y;
        uint16_t __w;
        uint16_t __h;

};


class Client {

    public:

        Client(int pipes[2])
            : __pipe_write(pipes[1])
            , __pipe_read(pipes[0]) { }


        const ssize_t recv(void* buffer, size_t size) const {
            return read(__pipe_read, buffer, size);
        }

        const ssize_t send(const void* buffer, size_t size) const {
            return write(__pipe_write, buffer, size);
        }

    private:
        int __pipe_write;
        int __pipe_read;

};


struct {

    struct {
        int kbd;
        int mouse;
    } input;

    struct {

        std::string path;

        struct fb_var_screeninfo var;
        struct fb_fix_screeninfo fix;

    } screen;

    #define SCREEN_PRIMARY (&context.screen)


    std::vector<Client> clients {};

} context;


static void show_usage(int argc, char** argv) {
    printf(
        "Use: aplus-ui [options]...\n"
        "User Interface Server.\n\n"
        "Options:\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aplus coreutils) 0.1\n"
        "Copyright (c) %s Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __DATE__ + 7, __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}



int main(int argc, char** argv) {
    
    
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "vh", long_options, &idx)) != -1) {
        switch(c) {
            case 'v':
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



    int fd;

    if((fd = open("/dev/fb0", O_RDWR)) < 0) {
        perror("/dev/fb0");
        exit(1);
    }


    if(ioctl(fd, FBIOGET_VSCREENINFO, &SCREEN_PRIMARY->var) < 0) {
        perror("FBIOGET_VSCREENINFO");
        exit(1);
    }

    if(ioctl(fd, FBIOGET_FSCREENINFO, &SCREEN_PRIMARY->fix) < 0) {
        perror("FBIOGET_FSCREENINFO");
        exit(1);
    }

    SCREEN_PRIMARY->path = "/dev/fb0";

    close(fd);



    if((context.input.kbd = open("/dev/kbd", O_RDONLY)) < 0) {
        perror("/dev/kbd");
        exit(1);
    }

    if((context.input.mouse = open("/dev/mouse", O_RDONLY)) < 0) {
        perror("/dev/mouse");
        exit(1);
    }



    if((fd = mkfifo("/tmp/aplus-ui-server", 0666 | O_NONBLOCK)) < 0) {
        perror("/tmp/aplus-ui-server");
        exit(1);
    }

    
    while(1) {

        struct pollfd pfd[3] = {
            { context.input.kbd,    POLLIN, 0 },
            { context.input.mouse,  POLLIN, 0 },
            { fd,                   POLLIN, 0 }
        };


        if(poll(pfd, 3, -1) < 0) {
            perror("poll");
            exit(1);
        }


        if(pfd[0].revents & POLLIN) {
            // keyboard
        }

        if(pfd[1].revents & POLLIN) {
            // mouse
        }

        if(pfd[2].revents & POLLIN) {
            // server
        }

    }



    return 0;
    
}