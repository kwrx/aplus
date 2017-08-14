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


#if 0
    if(init_server(&dmx) != 0) {
        TRACE("init_server() failed!\n");
        return -1;
    }
#endif



    if(init_fontengine(&dmx) != 0) {
        TRACE("init_fontengine() failed!\n");
        return -1;
    }

    if(init_render(&dmx) != 0) {
        TRACE("init_render() failed!\n");
        return -1;
    }

    if(init_cursor(&dmx) != 0) {
        TRACE("init_cursor() failed!\n");
        return -1;
    }


    /* Test */
    static dmx_window_t wnd;
    wnd.x = 0;
    wnd.y = 0;
    wnd.w = 1280;
    wnd.h = 768;
    wnd.alpha = 1.0;
    wnd.flags = 0;
    wnd.next = NULL;
    wnd.surface = cairo_image_surface_create_from_png("/usr/share/images/wp.png");

    dmx.windows = &wnd;
    dmx_mark_window(&dmx, &wnd, NULL);


    //pthread_create(&dmx.th_server, NULL, th_server, &dmx);
    pthread_create(&dmx.th_render, NULL, th_render, &dmx);
    pthread_create(&dmx.th_cursor, NULL, th_cursor, &dmx);
    //pthread_join(dmx.th_server, NULL);
    pthread_join(dmx.th_render, NULL);
    pthread_detach(dmx.th_render);
    pthread_detach(dmx.th_cursor);


    unlink(DMX_PIPE);
    return 0;
}