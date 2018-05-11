#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/termios.h>


#include "sh.h"


void sh_prompt(char* user, char* host, int last_err) {
    struct winsize ws;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);

    time_t t0 = time(NULL);
    struct tm* tm = localtime(&t0);

    char cwd[BUFSIZ] = { 0 };
    getcwd(cwd, BUFSIZ);

    char* home = getenv("HOME");
    if(home && (strstr(cwd, home) == cwd)) {
        strcpy(cwd + 1, &cwd[strlen(home) + 1]);
        cwd[0] = '~';
        cwd[strlen(home) + 1] = '\0';
    }

    fprintf(stdout, 
        "\e[%dC[\e[33m%02d/%02d \e[31m%02d:%02d:%02d\e[39m]\e[%dD",
        ws.ws_col - 17,
        tm->tm_mday,
        tm->tm_mon + 1,
        tm->tm_hour,
        tm->tm_min,
        tm->tm_sec,
        ws.ws_col - 1
    );

    fprintf(stdout, 
        "\e[36m[%s@%s %s]%c\e[39m ",
        user,
        host,
        cwd,
        geteuid() == 0 ? '#' : '$'
    );

    fflush(stdout);
}