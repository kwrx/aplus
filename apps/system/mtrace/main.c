#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/types.h>

#include <aplus/base.h>
#include <aplus/utils/async.h>

static FILE* fp = NULL;
static char pname[BUFSIZ];


static void show_usage(int argc, char** argv) {
    printf(
        "Use: mtrace [OPTIONS] [PID]\n"
        "Trace memory usage by a process\n\n"
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
    memset(pname, 0, sizeof(pname));


    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "hv", long_options, &idx)) != -1) {
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

    if(argc < optind)
        show_usage(argc, argv);

    
    static char buf[BUFSIZ];
    sprintf(buf, "/proc/%d/stat", atoi(argv[optind]));

    fp = fopen(buf, "r");
    if(!fp) {
        perror(buf);
        return -1;
    }

    int unused;
    fscanf(fp, "%d %s", &unused, &pname);
    fclose(fp);

    memset(buf, 0, sizeof(buf));
    sprintf(buf, "/proc/%d/statm", atoi(argv[optind]));

    fp = fopen(buf, "r");
    if(!fp) {
        perror(buf);
        return -1;
    }


    if(fork() == 0) {
        await(async_timer ( {
            struct winsize ws;
            ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);


            int vmm;
            int rss;
            fp = freopen(buf, "r", fp);
            fscanf(fp, "%d %d", &vmm, &rss);
            

            fprintf (
                stdout, "\e[s\e[%d;%df\e[K[%d] %s: RSS/VMM: %.1f/%.1f Mb\e[u",
                ws.ws_row - 1, ws.ws_col - strlen(pname) - 30, getpid(), pname, 
                (double) rss / 1048576.0, (double) vmm / 1048576.0
            );

            fflush(stdout);
        }, NULL, 1));
    } 


    fclose(fp);
    return 0;
}

