#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <getopt.h>
#include <fcntl.h>

static void show_usage(int argc, char** argv) {
    printf(
        "Use: insmod <filename> [ARGS]\n"
        "Run kernel module.\n\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus sysutils) 0.1\n"
        "Copyright (c) 2018 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}

static void die(char* s) {
    perror(s);
    exit(-1);
}


int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "", long_options, &idx)) != -1) {
        switch(c) {
            case 'v':
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


    struct stat st;
    if(stat(argv[optind], &st) != 0)
        die("insmod: stat()");

    int fd = open(argv[optind], O_RDONLY);
    if(fd < 0)
        die("insmod: open()");
    
    void* buffer = (void*) malloc(st.st_size);
    if(read(fd, buffer, st.st_size) != st.st_size)
        die("insmod: read()");

    close(fd);


    if(init_module(buffer, st.st_size, NULL) != 0)
        die("insmod: init_module()");

    free(buffer);
    return 0;
}