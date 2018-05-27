#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <aplus/base.h>
#include <aplus/sysconfig.h>


extern int httpd(int);


static void show_usage(int argc, char** argv) {
    printf(
        "Use: http\n"
        "HTTP Server.\n\n"
        "       --deamon                run as deamon [httpd]\n"
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


static void atsig_handler(int sig) {
    fprintf(stderr, "httpd: service stopped\n");
    exit(sig);
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
  
    signal(SIGTERM, atsig_handler);
    signal(SIGQUIT, atsig_handler);

    if(!deamon)
        exit(1);
    else {
        if(strcmp((const char*) sysconfig("httpd.enabled", "false"), "true") != 0) {
            fprintf(stderr, "httpd: deamon disabled by /etc/config\n");
            return 0;
        }

        if(strcmp(argv[0], "[httpd]") != 0)
            execl("/proc/self/exe", "[httpd]", "--deamon", NULL);
        
        setsid();
        chdir((const char*) sysconfig("httpd.root", "/srv/http"));


        int fd = open("/dev/log", O_WRONLY);
        if(fd >= 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
        }


        return httpd((int) sysconfig("httpd.port", 80));
    }

    return 0;
}