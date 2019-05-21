/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <unistd.h>
#include <dirent.h>
#include <getopt.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <pwd.h>
#include <fcntl.h>

#ifndef MAXNAMLEN
#define MAXNAMLEN    256
#endif

static void show_usage(int argc, char** argv) {
    printf (
        "ls - list files\n"
        "\n"
        "usage: %s [-lha] [path]\n"
        "\n"
        " -a     list all files (including . files)\n"
        " -l     use a long listing format\n"
        " -h     human-readable file sizes\n"
        " -?     show this help text\n"
        "\n", argv[0]
    );
}

int main(int argc, char** argv) {
    
    int show_hidden = 0;
    int human_readable = 0;
    int long_mode = 0;
    int print_dir = 0;
    
    
    if(argc > 1) {
        int c;
        while((c = getopt(argc, argv, "ahl?")) != -1) {
            switch(c) {
                case 'a':
                    show_hidden = 1;
                    break;
                case 'h':
                    human_readable = 1;
                    break;
                case 'l':
                    long_mode = 1;
                    break;
                case '?':
                    show_usage(argc, argv);
                    return 0;
            }
        }
        
        if(optind + 1 < argc)
            print_dir = 1;
    }
    

    void show_content(char* d, char* p) {
        if(p[0] == '.')
            if(!show_hidden)
                return;
                
        if(!long_mode)
            printf("%s\n", p);
        else {
            char buf[MAXNAMLEN];
            sprintf(buf, "%s/%s", d, p);
            
            struct stat st;
            if(lstat(buf, &st) != -1) {
                #define IS(x, y)                                    \
                    if(S_IS##x (st.st_mode)) { printf(y); }
                    
                IS(LNK, "l")
                else IS(CHR, "c")
                else IS(BLK, "b")
                else IS(DIR, "d")
                else
                    printf("-");
                    
                #undef IS
                #define IS(x, y, z)                                 \
                    printf((st.st_mode & x) ? y : z)
                    
                                    
                IS(S_IRUSR, "r", "-");
                IS(S_IWUSR, "w", "-");
                IS(S_ISUID, "s", (st.st_mode & S_IXUSR ? "x" : "-"));
                IS(S_IRGRP, "r", "-");
                IS(S_IWGRP, "w", "-");
                IS(S_IXGRP, "x", "-");
                IS(S_IROTH, "r", "-");
                IS(S_IWOTH, "w", "-");
                IS(S_IXOTH, "x", "-");
                
                #undef IS
                
                printf(" %d ", st.st_nlink);
                
                char* nuid = "unknown";
                char* ngid = "unknown";


                struct passwd* pwd;
                if((pwd = getpwuid(st.st_uid)) != NULL)
                    nuid = pwd->pw_name;
                if((pwd = getpwuid(st.st_gid)) != NULL)
                    ngid = pwd->pw_name;

                printf("%s %s ", nuid, ngid);
                

                register int s = st.st_size;
                if(S_ISLNK(st.st_mode)) {
                    struct stat vst;
                    if(stat(buf, &vst) == 0)
                        s = vst.st_size;
                }

                if(human_readable) {
                    if(s >= (1 << 20))
                        printf("%d.%1dM", s / (1 << 20), (s - (s / (1 << 20)) * (1 << 20)) / ((1 << 20) / 10));
                    else if(s >= (1 << 10))
                        printf("%d.%1dK", s / (1 << 10), (s - (s / (1 << 10)) * (1 << 10)) / ((1 << 10) / 10));
                    else
                        printf("%d", s);    
                } else
                    printf("%d", s);
                    
                    
                char timebuf[80];
                struct tm* tm = localtime(&st.st_mtime);
                strftime(timebuf, 80, "%b %d %H:%M", tm);
                
                printf(" %s", timebuf);
                printf(" %s", p);
                
                if(S_ISLNK(st.st_mode)) {
                    char linkbuf[MAXNAMLEN];
                    if(readlink(buf, linkbuf, MAXNAMLEN) > 0) {
                        printf(" -> %s", linkbuf);
                    }
                }
                
                printf("\n");
            }    
        }
    }
    
    void showdir(char* p) {
        struct stat st;
        if(stat(p, &st) != 0) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], p, strerror(errno));
            return;
        }
        
        if(!(S_ISDIR(st.st_mode))) {
            show_content(".", p);
            return;
        }
        
        DIR* d = opendir(p);
        if(!d) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], p, strerror(errno));
            return;
        }
        
        if(print_dir)
            printf("%s:\n", p);
        
        struct dirent* ent;
        while(ent = readdir(d))
            show_content(p, ent->d_name);
        
        if(print_dir)
            printf("\n");
            
        closedir(d);
    }
    
    if(optind >= argc || argc == 1)
        showdir(".");
    else
        while(optind++ < argc) {
            if(argc - optind > 0)
                print_dir = 1;

            showdir(argv[optind - 1]);
        }
            
    
    return 0;
}