#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "gnx.h"

int verbose = 0;

static void show_usage(int argc, char** argv) {
    printf(
        "Use: gnx [options]...\n"
        "Start GNX (GNX is Not X11) Desktop Server.\n\n"
        "   -r, --run [PATH]        run GUI Application in PATH\n"
        "   -k, --kill [NAME]       kill GUI Application by NAME\n"
        "   -l, --list              list all applications executed\n"
        "   -d, --display [N]       initialize desktop on display N\n"
        "   -v, --verbose           explains what it is doing\n"
        "       --help              show this help\n"
        "       --version           print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus) 0.1\n"
        "Copyright (c) 2016 Antonio Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}

int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "run", required_argument, NULL, 'r'},
        { "kill", required_argument, NULL, 'k'},
        { "list", no_argument, NULL, 'l'},
        { "display", required_argument, NULL, 'd'},
        { "verbose", no_argument, NULL, 'v'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'w'},
        { NULL, 0, NULL, 0 }
    };
    
    #define gnx_apps_kill(s) 0
    #define gnx_apps_run(s) 0
    #define gnx_apps_list(s) 0
    
 
    int display = 0;
    
    int c, idx;
    while((c = getopt_long(argc, argv, "rkldv", long_options, &idx)) != -1) {
        switch(c) {
            case 'r':
                return gnx_apps_run(optarg);
            case 'k':
                return gnx_apps_kill(optarg);
            case 'l':
                return gnx_apps_list(stdout);
            case 'd':
                display = atoi(optarg);
                break;
            case 'v':
                verbose = 1;
                break;
            case 'w':
                show_version(argc, argv);
                break;
            case 'h':
            case '?':
                //show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }
    
    if(optind >= argc)
        return gnx_init(display);
        
    show_usage(argc, argv);
}