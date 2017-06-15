#include "config.h"



static void show_usage(int argc, char** argv) {
    printf(
        "Use: gnx [OPTIONS...]\n"
        "Graphical UI Server\n\n"
        "   -k, --kill                  kill all running server\n"
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
    if(ld >= 0)
        dup2(ld, STDOUT_FILENO);

    
    init_clients();
    init_display();
    

    pthread_t t0, t1;
    pthread_create(&t0, NULL, th_clients, NULL);
    pthread_create(&t1, NULL, th_display, NULL);
    pthread_join(t0, NULL);
    pthread_detach(t1);


    unlink(GNX_PIPE);
    return 0;
}