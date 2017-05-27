#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <getopt.h>
#include <glob.h>


#include "sh.h"

#if HAVE_LOGIN
#   include "md5.h"
#endif



static char buf[BUFSIZ];


static void show_usage(int argc, char** argv) {
    printf(
        "Use: sh [options] ...\n"
        "aPlus Shell.\n\n"
        "   -c, --command               execute a command\n"
        "   -h, --help                  show this help\n"
        "   -v, --version               print version info and exit\n"
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



static void cmd_cd(char** argv) {
    char* p = argv[1];
    if(argv[1] == NULL)
        p = "/";

    if(chdir(p))
        perror(p);
}
    
    




int main(int argc, char** argv, char** env) {

    sh_alias("la", (void*) "ls -lh", SH_ALIAS_TYPE_STRING);
    sh_alias("cd", (void*) cmd_cd, SH_ALIAS_TYPE_FUNC);



    static struct option long_options[] = {
        { "command", no_argument, NULL, 'c'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    
 
    
    int c, idx;
    while((c = getopt_long(argc, argv, "chv", long_options, &idx)) != -1) {
        switch(c) {
            case 'c':
                sh_cmdline(argv[optind]);
                return 0;
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
    



        
    char* username;
    char* hostname = sh_gethostname();

    
#if HAVE_LOGIN
    username = sh_login(hostname);
#else
    username = strdup(getenv("USER"));
#endif



    do {
        CLRBUF();
        fprintf(stdout, "\033[36m[%s@%s %s]#\033[39m ", username, hostname, getcwd(buf, BUFSIZ));
        fflush(stdout);

        CLRBUF();
        if(fgets(buf, BUFSIZ, stdin) > 0) {
            buf[strlen(buf) - 1] = '\0';
            sh_cmdline(buf);
        }
    } while(1);
}