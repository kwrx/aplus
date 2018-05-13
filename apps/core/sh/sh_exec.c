#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <glob.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/wait.h>

#include <aplus/base.h>
#include <aplus/utils/list.h>

#include "sh.h"


void sh_reset_tty() {
    struct termios ios;

    pid_t pgrp = getpgrp();
    ioctl(STDIN_FILENO, TIOCSPGRP, &pgrp);
    ioctl(STDIN_FILENO, TIOCGETA, &ios);

    ios.c_lflag &= ~ISIG;
    ioctl(STDIN_FILENO, TIOCSETA, &ios);
}


static int run(int argc, char** argv, int in, int out) {
    int i;
    for(i = 0; sh_commands[i].cmd; i++) {
        if(strcmp(sh_commands[i].cmd, argv[0]) != 0)
            continue;

        int old_in = dup(STDIN_FILENO);
        int old_out = dup(STDOUT_FILENO);
        int old_err = dup(STDERR_FILENO);

        dup2(in, STDIN_FILENO);
        dup2(out, STDOUT_FILENO);
        dup2(out, STDERR_FILENO);

        int e = sh_commands[i].fn(argc, argv);

        dup2(old_in, STDIN_FILENO);
        dup2(old_out, STDOUT_FILENO);
        dup2(old_err, STDERR_FILENO);

        close(old_in);
        close(old_out);
        close(old_err);

        return e;
    }

    pid_t e = fork();
    switch(e) {
        case -1:
            perror("sh: fork()");
            return -1;
        
        case 0: {
            setpgrp();
            pid_t pgrp = getpgrp();
            ioctl(STDIN_FILENO, TIOCSPGRP, &pgrp);

            struct termios ios;
            ioctl(STDIN_FILENO, TIOCGETA, &ios);
            ios.c_lflag |= ISIG;
            ioctl(STDIN_FILENO, TIOCSETA, &ios);

            dup2(in, STDIN_FILENO);
            dup2(out, STDOUT_FILENO);
            dup2(out, STDERR_FILENO);

            execvp(argv[0], argv);
            perror(argv[0]);
            exit(100);
        }

        default: {
            int err, r;
            do {
                err = waitpid(-1, &r, 0);
            } while((err != -1) || (err == -1 && errno != ECHILD));
            
            sh_reset_tty();
            return r;
        }
    }

    return -1;
}




int sh_exec(char* command) {
    if(!command || strlen(command) == 0)
        return 0;


    static list(char*, args);


    char* p;
    char varbuf[BUFSIZ];
    char garbage[BUFSIZ];


    #define clear_args() {  \
        list_each(args, v)  \
            free(v);        \
        list_clear(args);   \
    }


    while(isblank(*command))
        command++;



    memset(varbuf, 0, sizeof(varbuf));
    memset(garbage, 0, sizeof(garbage));

    do {
        switch(*command) {
            case '\"':
            case '\'':
                if(!(p = strchr(command + 1, *command))) {
                    fprintf(stderr, "sh: syntax-error: expected %c at end of line\n", *command);
                    return -1;
                }

                strncat(garbage, command + 1, p - command - 1);
                command = p + 1;
                break;
            case '$':
                command++;
                while(isalnum(*command))
                    strncat(varbuf, command++, 1);

                if(getenv(varbuf))
                    strcat(garbage, getenv(varbuf));

                memset(varbuf, 0, sizeof(varbuf));
                break;
            case '*':
                *command = '\032';
            default:
                strncat(garbage, command++, 1);
                break;
        }
    } while(*command);


    for(p = strtok(garbage, " "); p; p = strtok(NULL, " ")) {
        char* s;
        if(!(s = strchr(p, '\032')))
            list_push(args, strdup(p));
        else {
            do {
                if(*s == '\032')
                    *s = '*';
            } while(*++s);

            glob_t gl;
            glob(p, GLOB_NOSORT | GLOB_TILDE, NULL, &gl);

            if(!gl.gl_pathc)
                list_push(args, strdup(p));
            else {
                int i;
                for(i = 0; i < gl.gl_pathc; i++)
                    list_push(args, strdup(gl.gl_pathv[i]));
            } 

            globfree(&gl);
        }
    }
    


    char** argv = (char**) calloc(sizeof(char*), list_length(args) + 1);
    if(!argv) {
        fprintf(stderr, "sh: out of memory!");
        return -1;
    }

    int argc = 0;
    list_each(args, v) {
        argv[argc] = (char*) calloc(1, strlen(v) + 1);
        if(!argv[argc]) {
            fprintf(stderr, "sh: out of memory!");
            return -1;
        }

        strcpy(argv[argc++], v);
    }
    

    int e = run(argc, argv, STDIN_FILENO, STDOUT_FILENO);



    while(argc >= 0)
        free(argv[argc--]);
    
    free(argv);
    clear_args();

    return e;
}