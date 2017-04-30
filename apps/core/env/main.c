#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>


#define SHOW_FLAG_NEWLINE       1
#define SHOW_FLAG_TAB           2
#define SHOW_FLAG_ALL           4


static void show_usage(int argc, char** argv) {
    printf(
        "Use: env [options]... [-] [NOME=VALORE]... [COMMAND [ARG]...]\n"
        "Set each NAME to VALUE in the environment and run COMMAND.\n\n"
        "   -i, --ignore-environment    start with an empty environment\n"
        "   -O, --null                  end each output line with NUL, not newline\n"
        "   -u, --unset                 remove variable from environment\n"
        "   -h, --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus coreutils) 0.1\n"
        "Copyright (c) 2016-2017 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}

int main(int argc, char** argv, char** envp) {

    static struct option long_options[] = {
        { "ignore-environment", no_argument, NULL, 'i'},
        { "null", no_argument, NULL, 'O'},
        { "unset", no_argument, NULL, 'u'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'r'},
        { NULL, 0, NULL, 0 }
    };
    
    
    
    int null_environ = 0;
    int null_lines = 0;
    
    int c, idx;
    while((c = getopt_long(argc, argv, "iOuh", long_options, &idx)) != -1) {
        switch(c) {
            case 'i':
                null_environ = 1;
                break;
            case 'O':
                null_lines = 1;
                break;
            case 'u': /* TODO */
                fprintf(stderr, "%s: --unset: not yet supported\n", argv[0]);
                return -1;
            case 'r':
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
    
    if(null_environ)
        envp = NULL;
    
    if(optind >= argc || argc == 1) {
        if(!envp)
            exit(0);
            
        int i;
        for(i = 0; envp[i]; i++)
            fprintf(stdout, "%s%s", envp[i], !null_lines ? "\n" : "");
            
        exit(0);
    }
       
    static char** envp_n = { NULL };
    return execve(argv[optind], &argv[optind], envp ? envp : envp_n);
}