/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>


static void show_usage(int argc, char** argv) {
    printf(
        "Use: echo [options]... [STRING]...\n"
        "Print STRING(s) to standard output.\n\n"
        "   -e                          process escape characters\n"
        "   -n                          no newline at end of output\n"
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
        { "version", no_argument, NULL, 'q'},
        { NULL, 0, NULL, 0 }
    };
    
    
    
    int newline = 1;
    int escape = 0;
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "ne", long_options, &idx)) != -1) {
        switch(c) {
            case 'n':
                newline = 0;
                break;
            case 'e':
                escape = 1;
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

    
    int i;
    for(i = optind; i < argc; i++) {
        int j;
        for(j = 0; argv[i][j]; j++) {
            if(argv[i][j] == '\\' && escape) {
                switch(argv[i][j + 1]) {
                    case 0:
                        break;
                    case 'n':
                        fprintf(stdout, "\n");
                        break;
                    case 't':
                        fprintf(stdout, "\t");
                        break;
                    case 'v':
                        fprintf(stdout, "\v");
                        break;
                    case 'r':
                        fprintf(stdout, "\r");
                        break;
                    case 'a':
                        fprintf(stdout, "\a");
                        break;
                    case 'e':
                        fprintf(stdout, "\e");
                        break;
                    case '\\':
                        fprintf(stdout, "\\");
                        break;
                    default:
                        fprintf(stdout, "%c", argv[i][j + 1]);
                        break;
                }
                
                if(argv[i][j + 1] == '\0')
                    break;
                    
                j++;
            } else
                fprintf(stdout, "%c", argv[i][j]);
        }
        
        if(i + 1 < argc)
            fprintf(stdout, " ");
    }


 
    if(newline)
        fprintf(stdout, "\n");
        
        
    fflush(stdout);
    return 0;
    
}