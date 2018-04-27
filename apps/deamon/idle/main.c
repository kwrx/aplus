#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <aplus/base.h>
#include <aplus/sysconfig.h>

static void show_usage(int argc, char** argv) {
    printf(
        "Use: idle\n"
        "System Idle Process.\n\n"
        "       --deamon                run as deamon [idle]\n"
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
  

    if(!deamon)
        fprintf(stderr, "idle: BOOOOOOOOOOM!!!!!!\n");
    else {
        if(strcmp((const char*) sysconfig("idle.enabled", "false"), "true") != 0) {
            fprintf(stderr, "idle: deamon disabled by /etc/config\n");
            return 0;
        }

        if(strcmp(argv[0], "[idle]") != 0)
            execl("/proc/self/exe", "[idle]", "--deamon", NULL);
        
        
        
        FILE* fp = fopen("/dev/log", "w");
        if(fp)
            fprintf(fp, "idle: running as deamon\n");


        int p;
        if((p = (int) sysconfig("idle.priority", 0)) > 0) {
            switch(p) {
                case 1:
                    nice(19);
                    break;
                case 2:
                    break;
                case 3:
                    nice(-20);
                    break;
            }
        }
            
        sleep(5); /* FIXME */
        for(;;)
#if defined(__i386__) || defined(__x86_64__)
            __builtin_ia32_pause()
#endif
        ;
    }

    return 0;
}