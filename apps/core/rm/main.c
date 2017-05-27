#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <getopt.h>


static void show_usage(int argc, char** argv) {
    printf(
        "Use: rm [options]... [FILE]...\n"
        "Remove (unlink) the FILE(s).\n\n"
        "   -f, --force                 ignore nonexistent files and arguments\n"
        "   -r, -R, --recursive         remove directories and ther contents recursively\n"
        "   -d, --dir                   remove empty directories\n"
        "   -v, --verbose               explain what is being done\n"
        "       --help                  show this help\n"
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
        { "force", no_argument, NULL, 'f'},
        { "recursive", no_argument, NULL, 'r'},
        { "dir", no_argument, NULL, 'd'},
        { "verbose", no_argument, NULL, 'v'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'q'},
        { NULL, 0, NULL, 0 }
    };
    
    
    
    int force = 0;
    int rmmode = 0; /* 1: recursive, 2: empty dir */
    int verbose = 0;
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "frRdv", long_options, &idx)) != -1) {
        switch(c) {
            case 'f':
                force = 1;
                break;
            case 'r':
            case 'R':
                rmmode = 1;
                break;
            case 'd':
                rmmode = 2;
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
        
        
    void rm(char* file) {
        if(unlink(file) != 0) {
            if(force)
                return;
              
            fprintf(stderr, "%s: %s: can not remove: %s\n", argv[0], file, strerror(errno));
            exit(-1);
        }
        
        if(verbose)
            fprintf(stdout, "%s: removed\n", file);
    }

    int rm_r(char* dir) {
        DIR* d = opendir(dir);
        if(!d) {
            if(force)
                return -1;
                
            fprintf(stderr, "%s: %s: %s\n", argv[0], dir, strerror(errno));
            exit(-1);
        }
        
        struct dirent* ent;
        while((ent = readdir(d))) {
            char* buf = (char*) calloc(BUFSIZ, 1);
            sprintf(buf, "%s/%s", dir, ent->d_name);
            
            struct stat st;
            if(lstat(buf, &st) != 0) {
                free(buf);
                
                if(force)
                    return -1;
                    
                fprintf(stderr, "%s: %s: can not remove: %s\n", argv[0], buf, strerror(errno));
                exit(-1);
            }
            
            if(S_ISDIR(st.st_mode)) {
                if(rm_r(buf) != 0) {
                    free(buf);
                    
                    fprintf(stderr, "%s: %s: can not remove: %s\n", argv[0], buf, strerror(errno));
                    exit(-1);
                }
            } else
                rm(buf);
            
            free(buf);
            rewinddir(d);
        }
        
        closedir(d);
        rm(dir);
        
        return 0;
    }

    void rm_d(char* dir) {
        DIR* d = opendir(dir);
        if(!d) {
            if(force)
                return;
                
            fprintf(stderr, "%s: %s: %s\n", argv[0], dir, strerror(errno));
            exit(-1);
        }
        
        struct dirent* ent = readdir(d);
        closedir(d);
        
        if(ent) {
            if(force)
                return;
                
            fprintf(stderr, "%s: %s: %s\n", argv[0], dir, strerror(ENOTEMPTY));
            exit(-1);
        }
        
        rm(dir);
    }
        
        
    int i;
    for(i = optind; i < argc; i++) {
        struct stat st;
        if(lstat(argv[i], &st) != 0) {
            if(force)
                continue;
                
            fprintf(stderr, "%s: %s: %s\n", argv[0], argv[i], strerror(errno));
            exit(-1);
        }
        
        if(S_ISDIR(st.st_mode)) {
            switch(rmmode) {
                case 0:
                    fprintf(stderr, "%s: %s: %s\n", argv[0], argv[i], strerror(EISDIR));
                    exit(-1);
                case 1:
                    rm_r(argv[i]);
                    break;
                case 2:
                    rm_d(argv[i]);
                    break;
            }
        } else 
            rm(argv[i]);
    }
    
    return 0;
}
