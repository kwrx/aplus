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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __BSD_VISIBLE
#undef __BSD_VISIBLE
#endif
#define __BSD_VISIBLE 1

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>


static void show_usage(int argc, char** argv) {
    printf(
        "Use: chroot [options]... NEWROOT [COMANDO [ARG]...]\n"
        "Run COMMAND with directory root set by NEWROOT.\n\n"
        "       --skip-chdir            do not change working directory to \'/\'\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n\n"
        "If no command is given, run ${SHELL}.\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus coreutils) 0.1.%s\n"
        "Copyright (c) %s Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], COMMIT, __DATE__ + 7, __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}




int main(int argc, char** argv) {
    
    if(argc < 2)
        show_usage(argc, argv);
    
    static struct option long_options[] = {
        { "skip-chdir", no_argument, NULL, 's'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'q'},
        { NULL, 0, NULL, 0 }
    };
    
    
   
    
    int c, idx;
    while((c = getopt_long(argc, argv, "", long_options, &idx)) != -1) {
        switch(c) {
            case 's':
                /* TODO */
                break;
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
    
    if(optind >= argc)
        show_usage(argc, argv);
    
    

    if(chroot(argv[optind]) != 0) {
        fprintf(stderr, "%s: %s: %s\n", argv[0], argv[optind], strerror(errno));
        exit(-1);
    }
    
    if(optind + 1 > argc)
        exit(execvp(argv[optind + 1], &argv[optind + 1]));
    else
        exit(execlp("sh", "sh", NULL));


     
    return 0;
}