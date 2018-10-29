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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>



static void show_usage(int argc, char** argv) {
    printf(
        "Use: free [options]\n"
        "Display amount of free and used memory in the system.\n\n"
        "   -b, --bytes                 show output in bytes\n"
        "   -k, --kilo                  show output in kilobytes\n"
        "   -m, --mega                  show output in megabytes\n"
        "   -g, --giga                  show output in gigabytes\n"
        "   -h, --human                 show human-readable output\n"
        "       --si                    use powers of 1000 not 1024\n"
        "   -t, --total                 show total for RAM\n"
        "   -s N, --seconds N           repeat printing every N seconds\n"
        "   -c N, --count N             repeat printing N times, then exit\n"
        "   -h, --help                  show this help\n"
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


static char* fmt(double k, int d, int p) {
    int n = (int) k;
    while(d--)
        n /= p;

    char buf[32];
    sprintf(buf, "%d", n);
    return strdup(buf);
}

static char* human(double n, int p) {
    if(n < p)
        return fmt(n, 0, p);

    int i = 0;
    do {
        n /= p;
        i++;
    } while(n > p);

    char buf[32];
    sprintf(buf, "%8.3g%c", n, " KMG"[i]);
    return strdup(buf);
}


static double parse(FILE* fp, char* line) {
    static char buf[BUFSIZ];
    fseek(fp, 0, SEEK_SET);

    while(fgets(buf, BUFSIZ, fp)) {
        if(strncmp(buf, line, strlen(line)) != 0)
            continue;

        char* p = &buf[strlen(line) + 2];
        while(*p == ' ')
            p++;

        return (double) atoi(p) * 1024.0;
    }

    return 0.0;
}

int main(int argc, char** argv, char** envp) {

    static struct option long_options[] = {
        { "bytes", no_argument, NULL, 'b'},
        { "kilo", no_argument, NULL, 'k'},
        { "mega", no_argument, NULL, 'm'},
        { "giga", no_argument, NULL, 'g'},
        { "human", no_argument, NULL, 'h'},
        { "si", no_argument, NULL, 'p'},
        { "total", no_argument, NULL, 't'},
        { "seconds", required_argument, NULL, 's'},
        { "count", required_argument, NULL, 'c'},
        { "help", no_argument, NULL, 'q'},
        { "version", no_argument, NULL, 'r'},
        { NULL, 0, NULL, 0 }
    };
    

    int d = 0;
    int p = 1024;
    int seconds = 0;
    int count = 1;
    int tt = 0;

    int c, idx;
    while((c = getopt_long(argc, argv, "bkmghps:c:", long_options, &idx)) != -1) {
        switch(c) {
            case 'b':
                d = 0;
                break;
            case 'k':
                d = 1;
                break;
            case 'm':
                d = 2;
                break;
            case 'g':
                d = 3;
                break;
            case 'h':
                d = -1;
                break;
            case 'p':
                p = 1000;
                break;
            case 't':
                tt = 1;
                break;
            case 's':
                seconds = atoi(optarg);
                count = 99999999;
                break;
            case 'c':
                count = atoi(optarg);
                break;
            case 'r':
                show_version(argc, argv);
                break;
            case 'q':
            case '?':
                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }


    FILE* fp = fopen("/proc/meminfo", "r");
    if(!fp) {
        perror("/proc/meminfo");
        return -1;
    }
    
    double tot = parse(fp, "MemTotal");
    double usd = parse(fp, "MemUsed");
    double avl = parse(fp, "MemAvailable");
    double fre = parse(fp, "MemFree");

    fclose(fp);


    int i;
    for(i = 0; i < count; i++) {
        fprintf(stdout, "              total        used        free      shared  buff/cache   available\n");
        
        
        if(d == -1)
            fprintf(stdout, "Mem:      %-12s%-12s%-12s%-12s%-12s%-12s\n", human(tot, p), human(usd, p), human(fre, p), "0", "0", human(avl, p));
        else
            fprintf(stdout, "Mem:      %-12s%-12s%-12s%-12s%-12s%-12s\n", fmt(tot, d, p), fmt(usd, d, p), fmt(fre, d, p), "0", "0", fmt(avl, d, p));


        if(seconds)
            sleep(seconds);
    }

    return 0;   
}