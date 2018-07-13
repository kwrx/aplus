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
#include <unistd.h>
#include <sys/types.h>
#include <getopt.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>

#include <aplus/base.h>
#include <aplus/sysconfig.h>

#include "sh.h"



static void show_usage(int argc, char** argv) {
    printf(
        "Use: xsh [options]\n"
        "aPlus eXperimental Shell\n"
        "   -c, --command <command>     pass a single command to the shell with -c"
        "   -h, --help                  show this help\n"
        "   -v, --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus coreutils) 0.1\n"
        "Copyright (c) 2016 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}


static char* getuser() {
    char* s = getenv("USER");
    if(s)
        return strdup(s);

    struct passwd* pw = getpwuid(getuid());
    if(!pw)
        return strdup("unknown");

    return strdup(pw->pw_name);
}

static char* gethost() {
    struct utsname u;
    if(uname(&u) != 0)
        return strdup("unknown");

    return strdup(u.nodename);
}



int main(int argc, char** argv, char** env) {
    
    static struct option long_options[] = {
        { "command", required_argument, NULL, 'c'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };


    
    char* command = NULL;

    int c, idx;
    while((c = getopt_long(argc, argv, "c:hv", long_options, &idx)) != -1) {
        switch(c) {
            case 'c':
                if(strcmp(optarg, "-c") != 0)
                    command = strdup(optarg);
                else
                    command = strdup(argv[optind]);
                break;
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


    char buf[BUFSIZ];
    setenv("PWD", getcwd(buf, BUFSIZ), 1);


    if(command)
        sh_exec(command);

    
    setsid();
    sh_reset_tty();



    char* user = getuser();
    char* host = gethost();


    int e = 0;
    do {
        char line[BUFSIZ];
        memset(line, 0, sizeof(line));

        if(!sh_prompt(line, user, host, e))
            continue;
        
        sh_history_add(line);
        e = sh_exec(line);
    } while(1);

    return 0;
}