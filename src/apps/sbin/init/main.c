/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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


#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>

#include <pthread.h>



static void init_welcome(void) {

    FILE* fp = fopen("/etc/motd", "r");
    if(!fp)
        return;

    char line[BUFSIZ];
    while(fgets(line, sizeof(line), fp) > 0)
        fprintf(stdout, line);


    fflush(stdout);
    fclose(fp);

}


static void init_environment(void) {

    fprintf(stderr, "open env\n");
    FILE* fp = fopen("/etc/environment", "r");
    if(!fp)
        return;
    fprintf(stderr, "success env\n");


    char line[BUFSIZ];
    while(fgets(line, sizeof(line), fp) > 0) {

        if(line[strlen(line) - 1] == '\n')
            line[strlen(line) - 1] = '\0';

        
        switch(line[0]) {

            case '\0':
            case  '#':
                continue;

            default:
                fprintf(stderr, "putenv(%s)\n", line);
                putenv(line);
                break;

        }

    }


    fclose(fp);

}


static void init_initd(void) {
    return;
}
    

int main(int argc, char** argv, char** envp) {

    signal(SIGTERM, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);


    int fd = open("/dev/kmsg", O_RDWR);
    if(fd < 0)
        return 1;

    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);


#if defined(DEBUG)
    if(getpid() != 1)
        fprintf(stderr, "init: WARN! getpid() != 1\n");
#endif


    setsid();
    tcsetpgrp(STDIN_FILENO, getpgrp());


    init_welcome();
    init_environment();
    init_initd();


    //for(; errno != ECHILD;)
    //    waitpid(-1, NULL, 0);
    

    fprintf(stderr, "init: unreachable point! system halted!\n");

    for(;;)
        ;//pause();

}