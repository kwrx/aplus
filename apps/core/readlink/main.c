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
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <getopt.h>


static void show_usage(int argc, char** argv) {
    printf(
        "Use: readlink [options]... FILE...\n"
        "Print value of symbolic link.\n\n"
        "   -f, --canonicalize          canonicalize by following every symlink\n"
        "   -n, --no-newline            do not output the trailing delimiter\n"
        "   -q, --quiet,                \n"
        "   -s, --silent                suppress most error messages\n"
        "   -v, --verbose               explain what is being done\n"
        "   -z, --zero                  end each output line with NUL, not newline\n"
        "       --help                  show this help\n"
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




int main(int argc, char** argv) {
    
    if(argc < 2)
        show_usage(argc, argv);
    
    static struct option long_options[] = {
        { "canonicalize", no_argument, NULL, 'f'},
        { "no-newline", no_argument, NULL, 'n'},
        { "quiet", no_argument, NULL, 'q'},
        { "silent", no_argument, NULL, 's'},
        { "zero", no_argument, NULL, 'z'},
        { "verbose", no_argument, NULL, 'v'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'w'},
        { NULL, 0, NULL, 0 }
    };
    
    
    
    int canonicalize = 0;
    int verbose = 0;
    int newline = 1;
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "fnqszv", long_options, &idx)) != -1) {
        switch(c) {
            case 'f':
                canonicalize = 1;
                break;
            case 'n':
            case 'z':
                newline = 0;
                break;
            case 'q':
            case 's':
                verbose = -1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'w':
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
        
        
    int i;
    for(i = optind; i < argc; i++) {
        char buf[BUFSIZ];
        if(readlink(argv[i], buf, BUFSIZ) < 0) {
            if(verbose)
                fprintf(stderr, "%s: %s: %s\n", argv[0], argv[i], strerror(errno));
        
            continue;
        }
        
        fprintf(stdout, "%s", buf);
        if(newline)
            fprintf(stdout, "\n");
    }
        
    fflush(stdout);
    return 0;
}