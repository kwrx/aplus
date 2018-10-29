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
#include <errno.h>
#include <getopt.h>

/* Select KEYMAP here */
#include "keymaps/en-US.h"


static void show_usage(int argc, char** argv) {
    printf(
        "Use: kmgen\n"
        "Generate compiled KEYMAP in stdout.\n\n"
        "   -h, --help                  show this help\n"
        "   -v, --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus sysutils) 0.1\n"
        "Copyright (c) %s Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], COMMIT, __DATE__ + 7, __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}

int main(int argc, char** argv) {
    
  
    
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'r'},
        { NULL, 0, NULL, 0 }
    };
       

    int c, idx;
    while((c = getopt_long(argc, argv, "hr", long_options, &idx)) != -1) {
        switch(c) {
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



    FILE* fp = fdopen(fileno(stdout), "wb");
    if(!fp) {
        perror("stdout");
        return -1;
    }
    
    
    unsigned short zeros[NR_KEYS];
    memset(zeros, 0, sizeof(zeros));
        
    int i;
    for(i = 0; i < 16; i++) {
        if(key_maps[i])
            fwrite(key_maps[i], sizeof(unsigned short) * NR_KEYS, 1, fp);
        else
            fwrite(zeros, sizeof(unsigned short) * NR_KEYS, 1, fp);
    }
    
    return 0;
}
