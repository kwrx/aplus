#include "dmx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>



static void show_usage(int argc, char** argv) {
    printf(
        "Use: dmx [OPTIONS...]\n"
        "Desktop Manager Server\n\n"
        "   -k, --kill                  kill all running server\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus sysutils) 0.1\n"
        "Copyright (c) 2016 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}



int main(int argc, char** argv) {

   
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'q'},
        { NULL, 0, NULL, 0 }
    };


    int c, idx;
    while((c = getopt_long(argc, argv, "k", long_options, &idx)) != -1) {
        switch(c) {
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


    int ld = open("/dev/log", O_WRONLY);
    if(ld >= 0) {
        dup2(ld, STDOUT_FILENO);
        //dup2(ld, STDERR_FILENO);
    }


    dmx_t dmx;
    memset(&dmx, 0, sizeof(dmx_t));



    if(init_server(&dmx) != 0) {
        TRACE("init_server() failed!\n");
        return -1;
    }

    if(init_fontengine(&dmx) != 0) {
        TRACE("init_fontengine() failed!\n");
        return -1;
    }

    if(init_render(&dmx) != 0) {
        TRACE("init_render() failed!\n");
        return -1;
    }

    if(init_input(&dmx) != 0) {
        TRACE("init_cursor() failed!\n");
        return -1;
    }




    pthread_create(&dmx.th_server, NULL, th_server, &dmx);
    pthread_create(&dmx.th_render, NULL, th_render, &dmx);
    pthread_create(&dmx.th_input, NULL, th_input, &dmx);
    

    th_main((void*) &dmx);
    return 0;
}