#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/termios.h>


#include "sh.h"



static void sh_prompt_info(char* user, char* host) {
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



static void sh_prompt_history(char* line, char* user, char* host, int history) {
    if(history < 0)
        return;

    do {
        list_each_r(sh_history, h) {
            if(history--)
                continue;

            memset(line, 0, BUFSIZ);
            strcpy(line, h);

            fprintf(stdout, "\e[2K\n\e[1A");
            sh_prompt_info(user, host);
            fprintf(stdout, h);
            fflush(stdout);
            break;
        }
    } while(history >= 0);
}


char* sh_prompt(char* line, char* user, char* host, int last_err) {
    sh_prompt_info(user, host);

    int history = 0;
    int i = 0;

    struct termios ios;
    ioctl(STDIN_FILENO, TIOCGETA, &ios);
    ios.c_lflag &= ~(ICANON | ECHO);
    ioctl(STDIN_FILENO, TIOCSETA, &ios);



    uint8_t ch[4];
    do {
        read(STDIN_FILENO, ch, 1);

        if(history < 0)
            history = 0;

        switch(ch[0]) {
            case '\e':
                if(ch[1] != '[')
                    break;
                
                switch(ch[2]) {
                    case 'A':
                        sh_prompt_history(line, user, host, history++);
                        i = strlen(line);
                        break;
                    case 'B':
                        sh_prompt_history(line, user, host, history--);
                        i = strlen(line);
                        break;
                }
                break;
            case '\b':
                if(i > 0) {
                    line[--i] = '\0';

                    fprintf(stdout, "\b");
                }
                break;
            case 128 ... 255:
                line[i++] = ch[0];
                line[i++] = ch[1];
                line[i] = '\0';

                fprintf(stdout, "%c%c", ch[0], ch[1]);
                break;
            case 32 ... 127:
            case '\t':
                line[i++] = ch[0];
                line[i] = '\0';

                fprintf(stdout, "%c", ch[0]);
                break;
            case '\n':
                break;
            default:
                fprintf(stdout, "^%c", 'A' + ch[0] - 1);
                break;

        }

        fflush(stdout);
    } while(ch[0] != '\n' && ch[0] != '\0');

    if(line[i - 1] == '\n')
        line[--i] = '\0';
    




    ioctl(STDIN_FILENO, TIOCGETA, &ios);
    ios.c_lflag |= (ICANON | ECHO);
    ioctl(STDIN_FILENO, TIOCSETA, &ios);


    if(strlen(line))
        return line;

    return NULL;
}