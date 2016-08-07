#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <aplus/gnx.h>
#include "server/gnxsrv.h"

int verbose = 0;

static void show_usage(int argc, char** argv) {
    printf(
        "Use: gnx [options]...\n"
        "Start GNX (GNX is Not X11) Desktop Server.\n\n"
        "   -k, --kill-server         close GNX Server and all desktop\n"
        "       --open [N]            initialize desktop on display N\n"
        "       --close [N]           close desktop on display N\n"
        "       --select [N]          select display N as default\n"
        "       --load-resource [R]   load GNX unknown resource from R\n"
        "       --load-image [R]      load GNX image resource from R\n"
        "       --load-font [R]       load GNX font resource from R\n"
        "       --unload-resource [R] unload GNX loaded resource\n"
        "       --create-handle [H]   create handle with appname H\n"
        "       --close-handle [H]    close handle with appname H\n"
        "   -v, --verbose             explains what it is doing\n"
        "       --help                show this help\n"
        "       --version             print version info and exit\n"
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
        { "kill-server", no_argument, NULL, 'k'}, 
        { "open", required_argument, NULL, 'a'},
        { "close", required_argument, NULL, 'b'},
        { "select", required_argument, NULL, 'c'},
        { "load-resource", required_argument, NULL, 'd'},
        { "load-image", required_argument, NULL, 'e'},
        { "load-font", required_argument, NULL, 'f'},
        { "unload-resource", required_argument, NULL, 'g'},
        { "create-handle", required_argument, NULL, 'i'},
        { "close-handle", required_argument, NULL, 'l'},
        { "verbose", no_argument, NULL, 'v'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'z'},
        { NULL, 0, NULL, 0 }
    };
    
 
    int display = 0;
    
    
    #define gnx_send_command(x, y, z, w, h)                                     \
        {                                                                       \
            int fd;                                                             \
            if((fd = gnxctl_open()) < 0) {                                      \
                fprintf(stderr, "gnx: unable to access to GNX Server\n");       \
                return -1;                                                      \
            }                                                                   \
                                                                                \
            gnxctl_send(fd, x, y, z, w, h);                                     \
            gnxctl_close(fd);                                                   \
        } return 0
    
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "kv", long_options, &idx)) != -1) {
        switch(c) {
            case 'a':
                gnx_send_command(GNXCTL_TYPE_INIT_DISPLAY, 0, atoi(optarg), 0, NULL);
            case 'b':
                gnx_send_command(GNXCTL_TYPE_FINI_DISPLAY, 0, atoi(optarg), 0, NULL);
            case 'c':
                gnx_send_command(GNXCTL_TYPE_SELECT_DISPLAY, 0, atoi(optarg), 0, NULL);
            case 'd':
                gnx_send_command(GNXCTL_TYPE_LOAD_RESOURCE, 0, GNXRES_TYPE_UNKNOWN, strlen(optarg) + 1, optarg);
            case 'e':
                gnx_send_command(GNXCTL_TYPE_LOAD_RESOURCE, 0, GNXRES_TYPE_IMAGE, strlen(optarg) + 1, optarg);
            case 'f':
                gnx_send_command(GNXCTL_TYPE_LOAD_RESOURCE, 0, GNXRES_TYPE_FONT, strlen(optarg) + 1, optarg);
            case 'g':
                gnx_send_command(GNXCTL_TYPE_UNLOAD_RESOURCE, 0, 0, strlen(optarg) + 1, optarg);
            case 'i':
                gnx_send_command(GNXCTL_TYPE_CREATE_HWND, 0, -1, strlen(optarg) + 1, optarg);
            case 'l':
                gnx_send_command(GNXCTL_TYPE_CLOSE_HWND, 0, 0, strlen(optarg) + 1, optarg);
            case 'k':
                gnx_send_command(GNXCTL_TYPE_KILL_SERVER, 0, atoi(optarg), 0, NULL);
            case 'v':
                verbose = 1;
                break;
            case 'z':
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
        return gnxsrv_init(display);
        
    show_usage(argc, argv);
}