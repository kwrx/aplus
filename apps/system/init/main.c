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


#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/termio.h>
#include <sys/termios.h>

#include <aplus/base.h>
#include <aplus/input.h>
#include <aplus/kd.h>

#define SYSCONFIG_VERBOSE
#include <aplus/sysconfig.h>
#include <aplus/utils/unicode.h>

    

static void init_initd() {
    DIR* d = opendir("/etc/init.d");
    if(!d) {
        fprintf(stderr, "init: no /etc/init.d directory found!\n");
        return;
    }


    struct winsize ws;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);


    struct dirent* ent;
    while((ent = readdir(d))) {
        static char path[BUFSIZ];
        sprintf(path, "/etc/init.d/%s", ent->d_name);

        int e = -1;
        do {
            if(access(path, F_OK) != 0)
                break;
            
            pid_t pid = fork();
            if(pid == -1)
                break;
            else if(pid == 0) {
                if(execl(path, path, "start", NULL) < 0)
                    perror(path);

                exit(-1);
            }
            e = 0;
        } while(0);
        
        fprintf(stderr, "\e[37minit: starting \e[36m%s\e[37m \e[%dG[ %3s ]\n", ent->d_name, ws.ws_col - 10, e == 0 ? "\e[32mOK\e[37m" : "\e[31mERR\e[37m");
    }

    closedir(d);
}


static void init_console() {
    int fd = open("/dev/console", O_RDONLY);
    if(fd < 0) {
        perror("init: /dev/console");
        return;
    }

    if(ioctl(fd, KDSETMODE, KD_GRAPHICS) != 0)
        perror("init: console_ioctl()");
    
    close(fd);
}

static void init_welcome() {
    FILE* fp = fopen("/etc/motd", "r");
    if(!fp)
        return;

    char ln[BUFSIZ];
    while(fgets(ln, BUFSIZ, fp) > 0)
        fprintf(stdout, ln);

    fclose(fp);
}


static void init_environment() {
    FILE* fp = fopen("/etc/environment", "r");
    if(!fp)
        return;


    char ln[BUFSIZ];
    while(fgets(ln, BUFSIZ, fp) > 0) {
        if(ln[strlen(ln) - 1] == '\n')
            ln[strlen(ln) - 1] = '\0';

        switch(ln[0]) {
            case '\0':
            case '#':
                continue;

            default:
                putenv(ln);
                break;
        }
    }


    char* s;
    if(!(s = getenv("LANG")) || !strlen(s))
        setenv("LANG", (const char*) sysconfig("init.locale", "en-US"), 1);

    fclose(fp);
}


int main(int argc, char** argv) {
    if(getppid() != 1)
        return 1;

    signal(SIGTERM, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);


    fcntl(open("/dev/stdin", O_RDONLY), F_DUPFD, STDIN_FILENO);
    fcntl(open("/dev/stdout", O_WRONLY), F_DUPFD, STDOUT_FILENO);
    fcntl(open("/dev/stderr", O_WRONLY), F_DUPFD, STDERR_FILENO);

    setsid();
    tcsetpgrp(STDIN_FILENO, getpgrp());


    init_console();
    init_welcome();
    init_environment();
    init_initd();


    ioctl(STDIN_FILENO, TIOCLKEYMAP, (void*) sysconfig("init.locale", "en-US"));
    
    for(; errno != ECHILD; )
        waitpid(-1, NULL, 0);

    return 0;
}