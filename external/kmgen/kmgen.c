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
        "Copyright (c) 2017 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
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
