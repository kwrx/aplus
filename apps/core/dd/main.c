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
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>


static int ibs = 512;
static int obs = 512;
static int count = INT_MAX;
static int seek = 0;
static int skip = 0;
static int status = 0;
static int progress = 0;

static struct timeval progress_ce;
static struct timeval progress_cs;

static FILE* out;
static FILE* in;





static void show_usage(int argc, char** argv) {
    printf(
        "Use: dd [options]...\n"
        "Copy a file, converting and formatting according to the operands.\n\n"
        "   bs=BYTES                    read and write up to BYTES bytes at a time\n"
        "   count=N                     copy only N input blocks\n"
        "   ibs=BYTES                   read up to BYTES bytes at a time (default: 512)\n"
        "   if=FILE                     read from FILE instead of stdin\n"
        "   obs=BYTES                   write BYTES bytes at a time (default: 512)\n"
        "   of=FILE                     write to FILE instead of stdout\n"
        "   seek=N                      skip N obs-sized blocks at start of output\n"
        "   skip=N                      skip N ibs-sized blocks at start of input\n"
        "   status=LEVEL                the LEVEL of information to print to stderr;\n"
        "                                   'none' suppresses everything by error messages,\n"
        "                                   'noxfer' suppresses the final transfer statistics,\n"
        "                                   'progress' shows periodic transfer statistics\n"
        "\n"
        "N and BYTES may be followed by the following multiplicative suffixes:\n"
        "c =1, w =2, b =512, kB =1000, K =1024, MB =1000*1000, M =1024*1024, xM =M,\n"
        "GB =1000*1000*1000, G =1024*1024*1024, and so on for T, P, E, Z, Y.\n"
        "\n"
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




static int parse_number(char* s) {
    char* p;
    if(isdigit(s[strlen(s) - 1])) {
        if((p = strchr(s, 'x')))
            return atoi(s) * atoi(++p);
        
        return atoi(s);
    }

    switch(s[strlen(s) - 1]) {
        #define __case(x, y)    \
            case x: return atoi(s) * y 

        __case('c', 1);
        __case('w', 2);
        __case('b', 512);
        __case('K', 1024);
        __case('M', 1024 * 1024);
        __case('G', 1024 * 1024 * 1024);
        
        case 'B':
            switch(s[strlen(s) - 2]) {
                __case('k', 1000);
                __case('M', 1000 * 1000);
                __case('G', 1000 * 1000 * 1000);
                default:
                    break;
            }

        default:
            fprintf(stderr, "dd: invalid number: \"%s\"\n", s);
            exit(-1);
    }
}


static char* sn(long n, long m) {
    if(n == 0)
        return "0";

    int i, j;
    for(i = 1, j = 0; n / i > m; j++)
        i *= m;

    if(j > 3)
        return "<overflow>";

    char buf[8];
    sprintf(buf, "%ld%c", n / i, "\0kMG"[j]);

    return strdup(buf);
}


static void print_xfer() {
    fprintf(stderr, "%d+%d record in\n", progress, 0);
    fprintf(stderr, "%d+%d record out\n", progress, 0);
}

static void print_status() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    double tm = (double) (tv.tv_sec - progress_cs.tv_sec) + (double) (tv.tv_usec - progress_cs.tv_usec) / 1000000.0;
    tm = tm == 0.0 ? 0.01 : tm;

    fprintf(stderr, "%d bytes (%sB, %siB) copied, %4.2f s, %sB/s\n", progress * obs, sn(progress * obs, 1000), sn(progress * obs, 1024), tm, sn((long)((double) (progress * obs) / tm), 1024));
}


static void dd_sighand(int sig) {
    switch(sig) {
        case SIGUSR1:
        case SIGINT:
            if(status < 2)
                print_status();
            break;
        case SIGQUIT:
        case SIGTERM:
            if(status < 2) {
                print_xfer();
                print_status();
            }

            _exit(sig);
            break;
        default:
            break;
    }

    signal(sig, dd_sighand);
}



int main(int argc, char** argv) {
    
    
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    
    
    in = stdin;
    out = stdout;

    signal(SIGUSR1, dd_sighand);
    signal(SIGQUIT, dd_sighand);
    signal(SIGTERM, dd_sighand);
    signal(SIGINT, dd_sighand);
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "hv", long_options, &idx)) != -1) {
        switch(c) {
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


    
    int i;
    for(i = optind; i < argc; i++) {
        char* opt[2];
        int j = 0;

        char* p;
        for(p = strtok(argv[i], "="); p && j < 2; p = strtok(NULL, "="))
            opt[j++] = p;

        #define __if(x, y)   \
            if(strcmp(opt[0], x) == 0) { y = parse_number(opt[1]); break; }

        do {
            __if("bs", ibs = obs)
            __if("ibs", ibs = obs)
            __if("obs", obs = ibs)
            __if("count", count)
            __if("seek", seek)
            __if("skip", skip)

            if(strcmp(opt[0], "if") == 0) {
                in = fopen(opt[1], "rb");
                if(!in) {
                    perror(opt[1]);
                    exit(-1);
                }

                break;
            }

            if(strcmp(opt[0], "of") == 0) {
                out = fopen(opt[1], "wb");
                if(!out) {
                    perror(opt[1]);
                    exit(-1);
                }

                break;
            }

            if(strcmp(opt[0], "status") == 0) {
                if(strcmp(opt[1], "none") == 0)
                    status = 2;
                else if(strcmp(opt[1], "noxfer") == 0)
                    status = 1;
                else if(strcmp(opt[1], "progress") == 0)
                    status = 0;
                else {
                    fprintf(stderr, "dd: invalid status level: \"%s\"\n", opt[1]);
                    exit(-1);
                }

                break;
            }
        } while(0);
    }



    fseek(in, skip * ibs, SEEK_SET);
    fseek(out, seek * obs, SEEK_SET);

    void* bb = malloc(ibs);
    if(!bb) {
        perror("dd");
        exit(-1);
    }


    gettimeofday(&progress_cs, NULL);
    
    for(
        progress = 0;
        fread(bb, ibs, 1, in) > 0 && progress < count;
        progress++
    ) {
        if(fwrite(bb, obs, 1, out) <= 0) {
            perror("I/O");
            exit(-1);
        }

        if(!status) { 
            static time_t t0;
            if(time(NULL) != t0) {
                print_status();
                t0 = time(NULL);
            }
        }
    }

    gettimeofday(&progress_ce, NULL);



    fclose(in);
    fclose(out);
    

    if(status < 1)
        print_xfer();

    if(status < 2)
        print_status();

        
    return 0;
}