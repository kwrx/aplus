#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <aplus/base.h>
#include <aplus/sysconfig.h>

static void show_usage(int argc, char** argv) {
    printf(
        "Use: sync\n"
        "Synchronize cached writes to persistent storage.\n\n"
        "       --deamon                run as deamon [syncd]\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
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
        { "deamon", no_argument, NULL, 'd'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    
    
    int deamon = 0;
    int c, idx;
    while((c = getopt_long(argc, argv, "d", long_options, &idx)) != -1) {
        switch(c) {
            case 'd':
                deamon++;
                break;
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
  
    nice(19);


    if(!deamon)
        sync();
    else {
        if(strcmp((const char*) sysconfig("syncd.enabled", "false"), "true") != 0) {
            fprintf(stderr, "syncd: deamon disabled by /etc/config\n");
            return 0;
        }

        if(strcmp(argv[0], "[syncd]") != 0)
            execl("/proc/self/exe", "[syncd]", "--deamon", NULL);
        
        
        int s = (int) sysconfig("syncd.timeout", 10);
        
        FILE* fp = fopen("/dev/log", "w");
        if(fp)
            fprintf(fp, "syncd: running as deamon every %d seconds\n", s);


        for(;; sleep(s))
            sync();
    }

    return 0;
}