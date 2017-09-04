#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>


#include "keymaps/it-IT.h"
#include "keymaps/en-US.h"


struct {
    char* name;
    void* data;
} keymaps[] = {
    { "it-IT", &it_IT_keymap },
    { "en-US", &en_US_keymap },
    { NULL, NULL }
};


static void show_usage(int argc, char** argv) {
    printf(
        "Use: kmgen KEYMAP [OUTPUT]\n"
        "Generate given KEYMAP in OUTPUT.\n\n"
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
    
    if(argc < 2)
        show_usage(argc, argv);
    
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



    FILE* fp = stdout;
    void* keydata = NULL;


    if(optind >= argc)
        show_usage(argc, argv);


    int i;
    for(i = 0; keymaps[i].name; i++)
        if(strcmp(keymaps[i].name, argv[optind]) == 0)
            keydata = keymaps[i].data;

    if(!keydata) {
        fprintf(stderr, "kmgen: keymap not found!\n");
        return -1;
    }    
    
    if(++optind < argc)
        fp = fopen(argv[optind], "wb");

    if(!fp) {
        perror(argv[optind]);
        return -1;
    }
    
    fwrite(keydata, 1, 1024, fp);
    fclose(fp);

    return 0;
}
