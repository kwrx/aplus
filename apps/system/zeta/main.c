#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <avm.h>

#define _ZETA_MAIN_H
#include "zeta.h"



static void show_usage(int argc, char** argv) {
    printf(
        "Use: zeta [options]...\n"
        "Aplus Sandbox System.\n\n"
        "   -v, --verbose               explain what is being done\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus sysutils) 0.1\n"
        "Copyright (c) 2016 Antonio Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}


static void sighandler(int sig) {
	if(avm_initialized())
		athrow(NULL, "java/lang/InternalError", strsignal(sig));

	exit(sig);
}


int main(int argc, char** argv) {
    
    
    static struct option long_options[] = {
        { "verbose", no_argument, NULL, 'v'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'q'},
        { NULL, 0, NULL, 0 }
    };
    
    
    int verbose = 0;
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "qv", long_options, &idx)) != -1) {
        switch(c) {
            case 'v':
                verbose = 1;
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
    
    
	
	avm_init();
	avm_config_path_add("/usr/lib/avm");
	avm_config_path_add("/usr/local/lib/avm");
	avm_config_path_add("/usr/share/java");
	
	signal(SIGINT, sighandler);
	signal(SIGILL, sighandler);
	signal(SIGFPE, sighandler);
	signal(SIGSEGV, sighandler);
	
	
	ZETA_LIBRARY_LOAD();
 
	 
	if(avm_open_library("rt.jar") == J_ERR) {
		perror("rt.jar");
		return 0;
	}
	
	if(avm_open_library("zeta.jar") == J_ERR) {
		perror("zeta.jar");
		return 0;
	}
    
	
	avm_begin();
	__Z(Activity_Load) ("zCore");
    avm_end();
	
	return 0;
}