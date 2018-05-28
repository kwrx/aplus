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
#include <unistd.h>
#include <string.h>
#include <sys/utsname.h>

#define FLAG_SYSNAME  0x01
#define FLAG_NODENAME 0x02
#define FLAG_RELEASE  0x04
#define FLAG_VERSION  0x08
#define FLAG_MACHINE  0x10

#define FLAG_ALL (FLAG_SYSNAME|FLAG_NODENAME|FLAG_RELEASE|FLAG_VERSION|FLAG_MACHINE)



void show_usage(int argc, char** argv) {
    fprintf(stderr,
            "uname - Print system version information.\n"
            "\n"
            "usage: %s [-asnrvm]\n"
            "\n"
            " -a     Print the standard uname string we all love\n"
            " -s     Print kernel name\n"
            " -n     Print system name\n"
            " -r     Print kernel version number\n"
            " -v     Print the extra kernel version information\n"
            " -m     Print the architecture name\n"
            "\n", argv[0]);
    exit(1);
}

int main(int argc, char** argv) {
    static struct utsname u;
    memset(&u, 0, sizeof(u));

    int c;
    int flags = 0;
    int space = 0;

    while ((c = getopt(argc, argv, "ahmnrsv")) != -1) {
        switch (c) {
            case 'a':
                flags |= FLAG_ALL;
                break;
            case 's':
                flags |= FLAG_SYSNAME;
                break;
            case 'n':
                flags |= FLAG_NODENAME;
                break;
            case 'r':
                flags |= FLAG_RELEASE;
                break;
            case 'v':
                flags |= FLAG_VERSION;
                break;
            case 'm':
                flags |= FLAG_MACHINE;
                break;
            case 'h':
            default:
                show_usage(argc, argv);
                break;
        }
    }
    
    
    
    uname(&u);

    if (!flags) {
        /* By default, we just print the kernel name */
        flags = FLAG_SYSNAME;
    }

    if (flags & FLAG_SYSNAME) {
        if (space++) printf(" ");
        printf("%s", u.sysname);
    }

    if (flags & FLAG_NODENAME) {
        if (space++) printf(" ");
        printf("%s", u.nodename);
    }

    if (flags & FLAG_RELEASE) {
        if (space++) printf(" ");
        printf("%s", u.release);
    }

    if (flags & FLAG_VERSION) {
        if (space++) printf(" ");
        printf("%s", u.version);
    }

    if (flags & FLAG_MACHINE) {
        if (space++) printf(" ");
        printf("%s", u.machine);
    }

    printf("\n");
    fflush(stdout);
    return 0;
}