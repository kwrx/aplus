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
#include <unistd.h>
#include <sys/types.h>
#include <getopt.h>
#include <fcntl.h>
#include <pwd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>

#include <aplus/base.h>
#include <aplus/sysconfig.h>
#include <aplus/crypto/sha256.h>

static void show_usage(int argc, char** argv) {
    printf(
        "Use: login [<username>]\n"
        "Begin a session on the system.\n\n"
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



int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };

    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "", long_options, &idx)) != -1) {
        switch(c) {
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



    fprintf(stderr, "\e[0;39;49m");
    char* username = NULL;
    
    do {    
        if(optind >= argc) {
            static char buf[BUFSIZ];
            memset(buf, 0, sizeof(buf));

            while(strlen(buf) == 0) {
                fprintf(stderr, "username: ");
                scanf("%s", buf);
                fflush(stdin);
            }

            username = buf;
        } else
            username = argv[optind];
            
        struct passwd* pwd;
        if(!(pwd = getpwnam(username))) {
            fprintf(stderr, "login: invalid username, try again!\n");
            continue;
        }

        if(strlen(pwd->pw_passwd) > 0) {
            if(strcmp(pwd->pw_passwd, "x") == 0) {
                pwd->pw_passwd = NULL;

                FILE* fp = fopen("/etc/shadow", "r");
                if(!fp) {
                    perror("/etc/shadow");
                    exit(-1);
                }

                static char buf[BUFSIZ];
                memset(buf, 0, sizeof(buf));

                static char passwd[BUFSIZ];
                memset(passwd, 0, sizeof(passwd));

                static char logname[BUFSIZ];
                memset(logname, 0, sizeof(logname));

                while(fgets(buf, sizeof(buf), fp)) {
                    sscanf(buf, "%[^:]:%[^:]:", logname, passwd);

                    if(strcmp(logname, pwd->pw_name) == 0) {
                        pwd->pw_passwd = passwd;
                        break;
                    }
                }

                fclose(fp);
            }

            if(!pwd->pw_passwd || strcmp(pwd->pw_passwd, "*") == 0) {
                fprintf(stderr, "login: cannot log-in in this account, try another\n");
                continue;
            }


            if(strlen(pwd->pw_passwd) > 0) {
                static char buf[BUFSIZ];
                static char buf2[BUFSIZ];
                memset(buf, 0, sizeof(buf));

                while(strlen(buf) == 0) {
                    fprintf(stderr, "password: ");

                    struct termios ts;
                    ioctl(STDIN_FILENO, TIOCGETA, &ts);
                    ts.c_lflag &= ~ECHO;
                    ioctl(STDIN_FILENO, TIOCSETA, &ts);

                    scanf("%s", buf);

                    ioctl(STDIN_FILENO, TIOCGETA, &ts);
                    ts.c_lflag |= ECHO;
                    ioctl(STDIN_FILENO, TIOCSETA, &ts);

                    fprintf(stderr, "\n");
                }



                if(strcmp(sha256(buf), pwd->pw_passwd) != 0) {
                    fprintf(stderr, "login: invalid password, try again!\n");
                    continue;
                }
            }
        }

        if(setuid(pwd->pw_uid) != 0) {
            perror("setuid()");
            continue;
        }

        if(setgid(pwd->pw_gid) != 0) {
            perror("setgid()");
            continue;
        }



        setenv("USER", pwd->pw_name, 1);
        setenv("LOGNAME", pwd->pw_name, 1);
        setenv("HOME", pwd->pw_dir, 1);
        setenv("SHELL", pwd->pw_shell, 1);
        
        chdir(pwd->pw_dir);
        exit(execlp(pwd->pw_shell, pwd->pw_shell, NULL));
    } while(1);

    return 0;
}