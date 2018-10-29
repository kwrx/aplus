/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


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
        "   -y, --yes                   Yes!\n"
        "   -q, --quiet                 Do not show log on stdout\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus coreutils) 0.1.%s\n"
        "Copyright (c) %s Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], COMMIT, __DATE__ + 7, __VERSION__, __TIMESTAMP__
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
    while((c = getopt_long(argc, argv, "RsruUlqyivh", long_options, &idx)) != -1) {
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