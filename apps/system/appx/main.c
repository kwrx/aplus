#include "appx.h"


int verbose = 1;
int yes = 0;

static void show_usage(int argc, char** argv) {
    printf(
        "Use: appx [OPTIONS...] [APP]\n"
        "App Manager of aPlus\n\n"
        "   -R, --run                   Search and launch app from applications system directory\n"
        "   -s, --install               Search and download app from online repositories\n"
        "   -i, --install-from          Install app from local .appx file\n"
        "   -r, --remove                Uninstall and remove app files\n"
        "   -u, --update                Update local database\n"
        "   -U, --upgrade               Upgrade appliations\n"
        "   -l, --list                  Show installed applications\n"
        "   -y, --yes                   Yes!"
        "   -q, --quiet                 Do not show log on stdout\n"
        "       --help                  show this help\n"
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

int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "run", no_argument, NULL, 'R'},
        { "install", no_argument, NULL, 's'},
        { "install-from", no_argument, NULL, 'i'},
        { "remove", no_argument, NULL, 'r'},
        { "update", no_argument, NULL, 'u'},
        { "upgrade", no_argument, NULL, 'U'},
        { "list", no_argument, NULL, 'l'},
        { "quiet", no_argument, NULL, 'q'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    

    
    int action = 0;

    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "RsiruUlqhv", long_options, &idx)) != -1) {
        switch(c) {
            case 'R':
            case 's':
            case 'r':
            case 'u':
            case 'U':
            case 'l':
                fprintf(stderr, "appx: To do...\n");
                return -1;
            case 'q':
                verbose = 0;
                break;
            case 'y':
                yes = 1;
                break;
            case 'i':
                action = c;
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
     
    
    switch(action) {
        case 'i':
            appx_install_from(argv[optind]);
            break;
        default:
            show_usage(argc, argv);
    }
    

    return 0;
}