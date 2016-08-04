#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>


static void show_usage(int argc, char** argv) {
    printf(
        "Use: mkdir [options]... [DIRECTORY]...\n"
        "Create the DIRECTORY(ies), if they do not already exist.\n\n"
        "   -m, --mode=MODE             set file mode (as in chmod), not a=rwx - umask\n"
        "   -p, --parents               no error if existing, make parent directories as needed\n"
        "   -v, --verbose               explain what is being done\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus coreutils) 0.1\n"
        "Copyright (c) 2016 Antonio Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}




int main(int argc, char** argv) {
    
    if(argc < 2)
        show_usage(argc, argv);
    
    static struct option long_options[] = {
        { "mode", no_argument, NULL, 'f'},
        { "parents", no_argument, NULL, 'r'},
        { "verbose", no_argument, NULL, 'v'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'q'},
        { NULL, 0, NULL, 0 }
    };
    
    
    
    int mkparents = 0;
    int verbose = 0;
    int mode = 0666;
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "mpv", long_options, &idx)) != -1) {
        switch(c) {
            case 'm':
                /* TODO */
                break;
            case 'p':
                mkparents = 1;
                break;
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
    
    if(optind >= argc)
        show_usage(argc, argv);
 
    
    int i;
    for(i = optind; i < argc; i++) {
        int fd = open(argv[i], O_CREAT | O_EXCL | O_RDONLY, S_IFDIR | mode);
        if(fd < 0) {
            if(mkparents)
                continue;
                
            fprintf(stderr, "%s: %s: directory already exists\n", argv[0], argv[i]);
            exit(-1);
        }
        
        if(verbose)
            fprintf(stdout, "%s: directory \'%s\' created\n", argv[0], argv[i]);
        
        close(fd);
    }
 
    return 0;       
}