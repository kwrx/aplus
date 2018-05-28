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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>


#define SHOW_FLAG_NEWLINE       1
#define SHOW_FLAG_TAB           2
#define SHOW_FLAG_ALL           4


static void show_usage(int argc, char** argv) {
    printf(
        "Use: env [options]... [-] [NOME=VALORE]... [COMMAND [ARG]...]\n"
        "Set each NAME to VALUE in the environment and run COMMAND.\n\n"
        "   -i, --ignore-environment    start with an empty environment\n"
        "   -O, --null                  end each output line with NUL, not newline\n"
        "   -u, --unset                 remove variable from environment\n"
        "   -h, --help                  show this help\n"
        "       --version               print version info and exit\n"
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

int main(int argc, char** argv, char** envp) {

    static struct option long_options[] = {
        { "ignore-environment", no_argument, NULL, 'i'},
        { "null", no_argument, NULL, 'O'},
        { "unset", no_argument, NULL, 'u'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'r'},
        { NULL, 0, NULL, 0 }
    };
    
    
    
    int null_environ = 0;
    int null_lines = 0;
    
    int c, idx;
    while((c = getopt_long(argc, argv, "iOuh", long_options, &idx)) != -1) {
        switch(c) {
            case 'i':
                null_environ = 1;
                break;
            case 'O':
                null_lines = 1;
                break;
            case 'u': /* TODO */
                fprintf(stderr, "%s: --unset: not yet supported\n", argv[0]);
                return -1;
            case 'r':
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
    
    if(null_environ)
        envp = NULL;
    
    if(optind >= argc || argc == 1) {
        if(!envp)
            exit(0);
            
        int i;
        for(i = 0; envp[i]; i++)
            fprintf(stdout, "%s%s", envp[i], !null_lines ? "\n" : "");
            
        exit(0);
    }
       
    static char** envp_n = { NULL };
    return execve(argv[optind], &argv[optind], envp ? envp : envp_n);
}