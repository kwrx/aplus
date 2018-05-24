#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#include <aplus/base.h>
#include <aplus/utils/unicode.h>

#include "sh.h"


list(char*, sh_history);

void sh_history_add(char* line) {
    if(strlen(line))
        list_push(sh_history, strdup(line));
}


static void sh_prompt_info(char* user, char* host) {
    struct winsize ws;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);

    time_t t0 = time(NULL);
    struct tm* tm = localtime(&t0);

    char cwd[BUFSIZ] = { 0 };
    getcwd(cwd, BUFSIZ);

    char* home = getenv("HOME");
    if(home && (strstr(cwd, home) == cwd)) {
        strcpy(cwd + 1, &cwd[strlen(home)]);
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
    if(history <= 0)
        return;
    
    if(list_length(sh_history) == 0)
        return;

    do {
        list_each_r(sh_history, h) {
            if(--history)
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
    ios.c_lflag &= ~(ICANON | ECHO | ECHOE);
    ioctl(STDIN_FILENO, TIOCSETA, &ios);



    uint8_t ch;
    do {
        read(STDIN_FILENO, &ch, 1);

        if(history < 0)
            history = 0;

        switch(ch) {
            case '\e':
                read(STDIN_FILENO, &ch, 1);
                if(ch != '[')
                    break;
                
                read(STDIN_FILENO, &ch, 1);
                switch(ch) {
                    case 'A':
                        sh_prompt_history(line, user, host, ++history);
                        i = strlen(line);
                        break;
                    case 'B':
                        sh_prompt_history(line, user, host, --history);
                        i = strlen(line);
                        break;
                }
                break;
            case '\b':
            case '\x7f':
                if(i > 0) {
                    line[--i] = '\0';

                    fprintf(stdout, "\b");
                }
                break;
            case 128 ... 255:
                line[i++] = ch;
                line[i] = '\0';
                
                fprintf(stdout, "%c", ch);


                for(int i = 0; i < utf8_bytes(ch) - 1; i++) {
                    read(STDIN_FILENO, &ch, 1);
                    fprintf(stdout, "%c", ch);
                }

                break;
            case 32 ... 126:
            case '\t':
                line[i++] = ch;
                line[i] = '\0';

                fprintf(stdout, "%c", ch);
                break;
            case '\n':
                fprintf(stdout, "\n");
                break;
            default:
                fprintf(stdout, "^%c", 'A' + ch - 1);
                break;

        }

        fflush(stdout);
    } while(ch != '\n' && ch != '\0');


    ioctl(STDIN_FILENO, TIOCGETA, &ios);
    ios.c_lflag |= (ICANON | ECHO | ECHOE);
    ioctl(STDIN_FILENO, TIOCSETA, &ios);


    if(strlen(line))
        return line;

    return NULL;
}