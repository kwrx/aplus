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
#include <aplus/crypto/md5.h>

static void show_usage(int argc, char** argv) {
    printf(
        "Use: su [options] [-] [<username> [<argument>...]]\n"
        "Change the effective user ID and group ID to that of <user>.\n"
        "A mere - implies -l.  If <user> is not given, root is assumed.\n\n"
        "   -m, --preserve-environment  do not reset environment variables\n"
        "   -g, --group <group>         specify the primary group\n"
        "   -G, --supp-group <group>    specify a supplmental group\n"
        "   -l, --login                 make the shell a login shell\n"
        "   -c, --command <command>     pass a single command to the shell with -c"
        "   -h, --help                  show this help\n"
        "   -v, --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus coreutils) 0.1\n"
        "Copyright (c) 2016 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}




int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "preserve-environment", no_argument, NULL, 'm'},
        { "group", required_argument, NULL, 'g'},
        { "supp-group", required_argument, NULL, 'G'},
        { "su", no_argument, NULL, 'l'},
        { "command", required_argument, NULL, 'c'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    

    int preserve_environ = 0;
    char* command = NULL;
    
    int c, idx;
    while((c = getopt_long(argc, argv, "mpg:G:lc:hv", long_options, &idx)) != -1) {
        switch(c) {
            case 'm':
            case 'p':
                preserve_environ = 1;
                break;
            case 'g':
            case 'G':
                fprintf(stderr, "su: %s: option not yet supported\n", optopt);
                exit(-1);
            case 'l':
                return execlp("login", "login", NULL);
            case 'c':
                if(strcmp(optarg, "-c") != 0)
                    command = strdup(optarg);
                else
                    command = strdup(argv[optind]);
                break;
            case 'v':
                show_version(argc, argv);
                break;
            case 'h':
            case '?':
                if(optopt == '-')
                    return execlp("login", "login", NULL);

                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }

    fprintf(stderr, "\e[39;49m");
    char* username = NULL;

    if(optind >= argc || command)
        username = "root";
    else
        username = argv[optind];


    if(strcmp(username, "-") == 0)
        return execlp("login", "login", NULL);
    
      
    struct passwd* pwd;
    if(!(pwd = getpwnam(username))) {
        fprintf(stderr, "su: user %s does not exist\n", username);
        exit(-1);
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
            fprintf(stderr, "su: cannot log-in in this account, try another\n");
            exit(-1);
        }


        if(strlen(pwd->pw_passwd) > 0) {
            static char buf[BUFSIZ];
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


            if(strcmp(md5(buf), pwd->pw_passwd) != 0) {
                fprintf(stderr, "su: invalid password\n");
                exit(-1);
            }
        }
    }

    if(setuid(pwd->pw_uid) != 0) {
        perror("setuid()");
        exit(-1);
    }

    if(setgid(pwd->pw_gid) != 0) {
        perror("setgid()");
        exit(-1);
    }

    if(!preserve_environ) {
        setenv("PATH", "/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin:/bin", 1);
        setenv("LD_DEBUG", "all", 1);
        setenv("LD_DEBUG_OUTPUT", "/dev/log", 1);
        setenv("LD_LIBRARY_PATH", "/usr/lib:/usr/local/lib:/lib", 1);
        setenv("TERM", "linux", 1);
        setenv("TMPDIR", "/tmp", 1);
        setenv("LANG", (const char*) sysconfig("sys.locale", ""), 1);
        setenv("USER", pwd->pw_name, 1);
        setenv("LOGNAME", pwd->pw_name, 1);
        setenv("HOME", pwd->pw_dir, 1);
        setenv("SHELL", pwd->pw_shell, 1);
    }
    
    if(command)
        exit(execlp(pwd->pw_shell, pwd->pw_shell, "-c", command, NULL));
    else
        exit(execlp(pwd->pw_shell, pwd->pw_shell, NULL));

    return 0;
}