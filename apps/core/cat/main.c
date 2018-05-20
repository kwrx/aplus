#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>


#define SHOW_FLAG_NEWLINE       1
#define SHOW_FLAG_TAB           2
#define SHOW_FLAG_ALL           4



static void show_usage(int argc, char** argv) {
    printf(
        "Use: cat [options]... [FILE]...\n"
        "Concatenate FILE(s) to standard output.\n\n"
        "   -A, --show-all              equivalent to -vET\n"
        "   -b, --number-nonblank       number nonempty output lines, overrides -n\n"
        "   -n, --number                number all output lines\n"
        "   -E, --show-ends             display $ at end of each line\n"
        "   -e                          equivalent to -vE\n"
        "   -T, --show-tabs             display ^I as TAB char\n"
        "   -t                          equivalent to -vT\n"
        "   -v, --show-nonprinting      use notation ^ and M-\n"
        "   -h, --help                  show this help\n"
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
    
    if(argc < 2)
        show_usage(argc, argv);
    
    static struct option long_options[] = {
        { "show-all", no_argument, NULL, 'A'},
        { "number-nonblank", no_argument, NULL, 'b'},
        { "number", no_argument, NULL, 'n'},
        { "show-ends", no_argument, NULL, 'E'},
        { "show-tabs", no_argument, NULL, 'T'},
        { "show-nonprinting", no_argument, NULL, 'v'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'r'},
        { NULL, 0, NULL, 0 }
    };
    
    
    
    int show = 0;
    int lines = 0;
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "AbnEeTtvh", long_options, &idx)) != -1) {
        switch(c) {
            case 'A':
                show = SHOW_FLAG_NEWLINE | SHOW_FLAG_TAB | SHOW_FLAG_ALL;
                break;
            case 'b':
                lines = 1;
                break;
            case 'n':
                lines = 2;
                break;
            case 'E':
            case 'e':
                show &= SHOW_FLAG_NEWLINE;
                break;
            case 'T':
            case 't':
                show &= SHOW_FLAG_TAB;
                break;
            case 'v':
                show &= SHOW_FLAG_ALL;
                break;
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
    
    if(optind >= argc)
        show_usage(argc, argv);
        
    int i, ln = 1;
    for(i = optind; i < argc; i++) {
        FILE* fp = fopen(argv[i], "r");
        if(!fp) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], argv[i], strerror(errno));
            continue;
        }
        
        char buf[BUFSIZ];
        int nl = 1;
        while(fgets(buf, BUFSIZ, fp)) {
            int n;
            if(n = (buf[strlen(buf) - 1] == '\n'))
                buf[strlen(buf) - 1] = 0;
            
            if(nl && lines > 0) {
                switch(lines) {
                    case 1:
                        if(strlen(buf) == 0)
                            break;
                    case 2:
                        fprintf(stdout, "   %d ", ln++);
                        break;
                }
            }
              
            nl = n;
            
            int i;
            for(i = 0; buf[i]; i++) {
                if(buf[i] >= 32)
                    fprintf(stdout, "%c", buf[i]);
                else {
                    if(buf[i] == '\t' && show & SHOW_FLAG_TAB)
                        fprintf(stdout, "^I");
                    else if(buf[i] == '\r')
                        ;
                    else
                        if(show & SHOW_FLAG_ALL)
                            fprintf(stdout, "^%d", buf[i] & 0xFF);
                }
            }
            
            if(nl) {
                if(show & SHOW_FLAG_NEWLINE)
                    fprintf(stdout, "$");
                    
                fprintf(stdout, "\n");
            }
        }
        
        fclose(fp);
    }
    
    return 0;
}