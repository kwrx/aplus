#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>

static void show_usage(int argc, char** argv) {
    printf(
        "Use: cp [options]... [-T] SOURCE DESTINATION\n"
        " or: cp [OPTIONS]... SOURCE... DIRECTORY\n"
        " or: cp [OPTIONS]... -t DIRECTORY SOURCE\n"
        "Copy SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.\n\n"
        "   -f, --force             force copy removing destination file\n"
        "   -l, --link              create links instead to copy files\n"
        "   -s, --symbolic-link     creare symlinks instead to copy files\n"
        "   -v, --verbose           explains what it is doing\n"
        "   -h, --help              show this help\n"
        "       --version           print version info and exit\n"
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
        { "force", no_argument, NULL, 'f'},
        { "link", no_argument, NULL, 'l'},
        { "symbolic-link", no_argument, NULL, 's'},
        { "verbose", no_argument, NULL, 'v'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'r'},
        { NULL, 0, NULL, 0 }
    };
    
    
    int force_copy = 0;
    int linkmode = 0; /* 0: no-link, 1: link, 2: symlink */
    int verbose = 0;
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "flsvh", long_options, &idx)) != -1) {
        switch(c) {
            case 'f':
                force_copy = 1;
                break;
            case 'l':
                linkmode = 1;
                break;
            case 's':
                linkmode = 2;
                break;
            case 'v':
                verbose = 1;
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
    
    if(optind + 1 >= argc)
        show_usage(argc, argv);
        
      
    void fcpy(int s, int d, char* ss, char* ds) {
        
        if(verbose)
            printf("\"%s\" -> \"%s\"\n", ss, ds);
        
        switch(linkmode) {
            case 0: {
                #define CHUNK (4096 * 1024)
                static char buf[CHUNK];
                
                int r;
                do {
                    r = read(s, buf, CHUNK);
                    if(__builtin_expect(r < 0, 0))
                        fprintf(stderr, "%s: I/O: cannot read target input\n", argv[0]);
                    else if(__builtin_expect(r > 0, 1)) {
                        if(__builtin_expect(write(d, buf, r) != r, 0))
                            fprintf(stderr, "%s: I/O: cannot write target output\n", argv[0]);
                    }
                } while(r > 0);
                
                close(s);
                close(d);
            } break;
            
            case 1: {
                close(s);
                close(d);
                
                unlink(ds);
                
                if(link(ss, ds) != 0) {
                    fprintf(stderr, "%s: cannot create link of %s in %s\n", argv[0], ss, ds);
                    exit(-1);
                }
            } break;
            
            case 2: {
                close(s);
                close(d);
                
                unlink(ds);
                
                if(symlink(ss, ds) != 0) {
                    fprintf(stderr, "%s: cannot create symlink of %s in %s\n", argv[0], ss, ds);
                    exit(-1);
                }
            } break;
        }
    }
    
    
    if(argc - optind > 2) {
        struct stat st;
        if(stat(argv[argc - 1], &st) != 0 || !(S_ISDIR(st.st_mode))) {
            fprintf(stderr, "%s: target \"%s\" is not a directory\n", argv[0], argv[argc - 1]);
            exit(-1);
        }
        
        int i;
        for(i = optind; i < argc - 1; i++) {
            int s = open(argv[i], O_RDONLY);
            if(s < 0 || stat(argv[i], &st) != 0) {
                fprintf(stderr, "%s: %s: no such file or directory\n", argv[0], argv[i]);
                exit(-1);
            }
           
            char buf[BUFSIZ];
            sprintf(buf, "%s/%s", argv[argc - 1], basename(argv[i]));
            
            int d = open(buf, O_CREAT | O_TRUNC | O_RDWR, st.st_mode);
            if(d < 0) {
                fprintf(stderr, "%s: %s: cannot create target file\n", argv[0], buf);
                exit(-1);
            }
           
        
            fcpy(s, d, argv[i], buf);
        }
    } else {
        int s, d, m;
        struct stat st;
        char buf[BUFSIZ];
        char* ds = NULL;
        
        s = open(argv[optind], O_RDONLY);
        if(s < 0 || stat(argv[optind], &st) != 0) {
            fprintf(stderr, "%s: %s: no such file or directory\n", argv[0], argv[optind]);
            exit(-1);
        }
        
        m = st.st_mode;
        
        
        
        if(stat(argv[argc - 1], &st) != 0) {
            d = open(argv[argc - 1], O_CREAT | O_TRUNC | O_RDWR, m);
            
            if(d < 0) {
                fprintf(stderr, "%s: %s: cannot create target file\n", argv[0], argv[argc - 1]);
                exit(-1);
            }
            
            ds = argv[argc - 1];
        } else {
            if(S_ISDIR(st.st_mode)) {
                sprintf(buf, "%s/%s", argv[argc - 1], basename(argv[optind]));
                
                d = open(buf, O_CREAT | O_TRUNC | O_RDWR, m);
                if(d < 0) {
                    fprintf(stderr, "%s: %s: cannot create target file\n", argv[0], buf);
                    exit(-1);
                }
                
                ds = buf;
            } else {
                d = open(argv[argc - 1], O_CREAT | O_TRUNC | O_RDWR, m);
                if(d < 0) {
                    fprintf(stderr, "%s: %s: cannot create target file\n", argv[0], argv[argc - 1]);
                    exit(-1);
                }
                
                ds = argv[argc - 1];
            }
        }

        
        fcpy(s, d, argv[optind], ds);
    }
    
    return 0;
}