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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <aplus/base.h>
#include <aplus/fb.h>

static void show_usage(int argc, char** argv) {
    printf(
        "Use: horizons [<object_id>]\n"
        "Show information about <object_id> object from Horizons interface (JPL Nasa).\n\n"
        "   -q, --query NAME            Query info about NAME object\n"
        "   -i, --only-info             show only info about <object_id>\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus sysutils) 0.1\n"
        "Copyright (c) 2016 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}



static int horizons_setup() {
    fprintf(stderr, "horizons: get host from ssd.jpl.nasa.gov\n");

    struct hostent* e = (struct hostent*) gethostbyname("ssd.jpl.nasa.gov");
    if(!e)
        return -1;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0)
        return -1;

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(6775);
    memcpy(&sin.sin_addr.s_addr, e->h_addr_list[0], e->h_length);


    fprintf(stderr, "horizons: connect to ssd.jpl.nasa.gov:6775\n");

    if(connect(fd, (struct sockaddr*) &sin, sizeof(sin)) != 0)
        return -1;

    fprintf(stderr, "horizons: connected\n");


    char buf[BUFSIZ];
    read(fd, buf, sizeof(buf));

    fprintf(stderr, "horizons: ready!\n");

    write(fd, "10\r\n", 4);
        read(fd, buf, sizeof(buf));

    fprintf(stdout, "%s\n", buf);
    return fd;
}



int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    

    static int only_info = 0;
    static int only_query = 0;



    int c, idx;
    while((c = getopt_long(argc, argv, "", long_options, &idx)) != -1) {
        switch(c) {
            case 'i':
                only_info = 1;
                break;
            case 'q':
                only_query = 1;
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

    if(optind >= argc)
        show_usage(argc, argv);
    

    int fd = horizons_setup();
    if(fd < 0) {
        perror("horizons");
        return -1;
    }

    //if(only_query)
    //    return horizons_query_name(argv[optind]);

    //if(only_info)
    //    return horizons_query_info(atoi(argv[optind]));

    /*struct space_object* obj = horizon_query_position(argv[optind]);
    if(!obj) {
        fprintf(stderr, "\'%s\': object not found\n");
        horizons_close(fd);

        return -1;
    }*/

    /* TODO */


    return 0;
}