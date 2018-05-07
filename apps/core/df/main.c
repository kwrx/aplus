#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <getopt.h>


static void show_usage(int argc, char** argv) {
    printf(
        "Use: df [OPTIONS]... [FILE]...\n"
        "Show information about the file system on which each FILE resides,\n"
        "or all file systems by default.\n\n"
        "   -a, --all                   include pseudo, duplicate,\n"
        "                               inacessible file systems\n"
        "   -h, --human-readable        print sizes in powers of 1024 (e.g., 1023M)\n"
        "   -H, --si                    print sizes in powers of 1000 (e.g., 1.1G)\n"
        "   -i, --inodes                print information about inodes instead blocks\n"
        "       --sync                  invoke sync before getting usage info\n"
        "       --total                 elide all entries insignificant to available space,\n"
        "                               and produce grand total\n"
        "   -T, --print-type            print file system type\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus coreutils) 0.1\n"
        "Copyright (c) 2018 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}



static int all = 0;
static int pmode = 1;
static int inodes = 0;
static int total = 0;
static int ptype = 0;

static char* sn(double n, long m) {
    if(n == 0.0)
        return "0";

    int i, j;
    for(i = 1, j = 0; n / i > (double) m; j++)
        i *= (double) m;

    if(j > 3)
        return "<overflow>";

    char buf[8];
    sprintf(buf, "%.1f%c", n / i, "\0KMG"[j]);

    return strdup(buf);
}

static int pfsys(char* path, char* dev, char* fsys) {
    struct statvfs st;
    if(statvfs(path, &st) != 0) {
        perror(path);
        return -1;
    }

    if(strcmp(dev, "none") == 0)
        dev = fsys;

    
    switch(pmode) {
        case 1:
            fprintf(stdout, "%-13s", dev);
            
            if(ptype)
                fprintf(stdout, " %-8s ", fsys);

            fprintf(stdout, 
                "%12d %12d %12d %8d%%   %s\n",
                (int) (inodes ? st.f_files 
                              : (st.f_blocks * st.f_bsize) / 1024),

                (int) (inodes ? st.f_files - st.f_ffree
                              : ((st.f_blocks - st.f_bfree) * st.f_bsize) / 1024),

                (int) (inodes ? st.f_ffree
                              : (st.f_bfree * st.f_bsize) / 1024),

                (int) (inodes ? (st.f_files ? ((double) (st.f_files - st.f_ffree) / (double) st.f_files * 100.0) : 0)
                              : (st.f_blocks ? ((double) (st.f_blocks - st.f_bfree) / (double) st.f_blocks * 100.0) : 0)),
                path
            );
            break;
        default:
            fprintf(stdout, "%-13s", dev);
            
            if(ptype)
                fprintf(stdout, " %-8s ", fsys);

            fprintf(stdout, 
                "%12s %12s %12s %8d%%   %s\n",
                sn (inodes ? st.f_files 
                           : (st.f_blocks * st.f_bsize), pmode),

                sn (inodes ? st.f_files - st.f_ffree
                           : ((st.f_blocks - st.f_bfree) * st.f_bsize), pmode),

                sn (inodes ? st.f_ffree
                           : (st.f_bfree * st.f_bsize), pmode),

                (int) (inodes ? (st.f_files ? ((double) (st.f_files - st.f_ffree) / (double) st.f_files * 100.0) : 0)
                              : (st.f_blocks ? ((double) (st.f_blocks - st.f_bfree) / (double) st.f_blocks * 100.0) : 0)),
                path
            );
            break;
    }
    
    /* TODO: --total */
    return 0;
}


int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "all", no_argument, NULL, 'a'},
        { "human-readable", no_argument, NULL, 'h'},
        { "si", no_argument, NULL, 'H'},
        { "inodes", no_argument, NULL, 'i'},
        { "sync", no_argument, NULL, 's'},
        { "total", no_argument, NULL, 't'},
        { "print-type", no_argument, NULL, 'T'},
        { "help", no_argument, NULL, 'z'},
        { "version", no_argument, NULL, 'r'},
        { NULL, 0, NULL, 0 }
    };
    
    
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "ahHiT", long_options, &idx)) != -1) {
        switch(c) {
            case 'a':
                all = 1;
                break;
            case 'h':
                pmode = 1024;
                break;
            case 'H':
                pmode = 1000;
                break;
            case 'i':
                inodes = 1;
                break;
            case 's':
                sync();
                break;
            case 't':
                total = 1;
                break;
            case 'T':
                ptype = 1;
                break;
            case 'r':
                show_version(argc, argv);
                break;
            case '?':
                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }


    if(!inodes) {
        switch(pmode) {
            case 1:
                fprintf(stdout, "File system   ");
                if(ptype)
                    fprintf(stdout, "Type      ");
                fprintf(stdout, "1024-blocks         Used         Free      Use%%   Mounted on\n");
                break;
            default:
                fprintf(stdout, "File system   ");
                if(ptype)
                    fprintf(stdout, "Type      ");
                fprintf(stdout, "       Size         Used         Free      Use%%   Mounted on\n");
                break;
        }   
    } else {
        fprintf(stdout, "File system   "); 
        if(ptype)
            fprintf(stdout, "Type      ");
        fprintf(stdout, "     Inodes        IUsed        IFree     IUse%%   Mounted on\n");
    }


    if(optind < argc)
        return pfsys(argv[optind], "", "unknown");
    
    

    FILE* fp = fopen("/proc/mounts", "r");
    if(!fp) {
        perror("df: /proc/mounts");
        return -1;
    }
    
    while(!feof(fp)) {
        char dev[BUFSIZ];
        char path[BUFSIZ];
        char fsys[BUFSIZ];
        char flags[BUFSIZ];
        int tmp;

        memset(dev, 0, sizeof(dev));
        memset(path, 0, sizeof(path));
        memset(fsys, 0, sizeof(fsys));
        memset(flags, 0, sizeof(flags));

        if(!fscanf(fp, "%s %s %s %s %d %d\n", dev, path, fsys, flags, &tmp, &tmp))
            break;

        if(!all && strstr(flags, "bind"))
            continue;

        pfsys(path, dev, fsys);
    }

    fclose(fp);
    return 0;
}