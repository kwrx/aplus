#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <libgen.h>


static void show_usage(int argc, char** argv) {
    printf(
        "Use: basename NAME\n"
        "Print NAME with any leading directory components removed.\n\n"
        "   -z, --zero                  end each output line with NUL, not newline\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus coreutils) 0.1\n"
        "Copyright (c) 2018 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}




int main(int argc, char** argv) {
    
    if(argc < 2)
        show_usage(argc, argv);
    
    static struct option long_options[] = {
        { "zero", no_argument, NULL, 'z'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'q'},
        { NULL, 0, NULL, 0 }
    };
    

    int zero = 0;

    int c, idx;
    while((c = getopt_long(argc, argv, "z", long_options, &idx)) != -1) {
        switch(c) {
            case 'z':
                zero = 1;
                break;
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
    
    if(optind >= argc)
        show_usage(argc, argv);

    
    fprintf(stdout, "%s%c", basename(argv[optind]), zero ? '\0' : '\n');
    return 0;
}