#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#include "animation.h"

static int min_col;
static int max_col;
static int min_row;
static int max_row;
static struct winsize ws;

static void show_usage(int argc, char** argv) {
    printf(
        "Use: nyancat\n"
        "Terminal Nyancat\n\n"
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



static void SIGWINCH_handler(int sig) {

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

    min_col = (FRAME_WIDTH - ws.ws_col / 2) / 2;
    max_col = (FRAME_WIDTH + ws.ws_col / 2) / 2;
    min_row = (FRAME_HEIGHT - ws.ws_row - 1) / 2;
    max_row = (FRAME_HEIGHT + ws.ws_row - 1) / 2;

    signal(SIGWINCH, SIGWINCH_handler);
}

static void atexit_handler(int sig) {
    fprintf(stdout, "\e[0;39;49m");
    fprintf(stdout, "\e[2J\e[H");
    fflush(stdout);

    exit(0);
}



static int digits(int n) {
    int d = 0;
    int c = 0;

    if(n >= 0)
        for(c = 10; c <= n; c *= 10)
            d++;
    else
        for(c = -10; c >= n; c *= 10)
            d++;

    return (c < 0) ? ++d : d;
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


    signal(SIGWINCH, SIGWINCH_handler);
    signal(SIGINT, atexit_handler);
    signal(SIGQUIT, atexit_handler);
    signal(SIGTERM, atexit_handler);
    signal(SIGKILL, atexit_handler);

    
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);



    static char* colors[256];
    colors[',']  = "\033[2;44m";    /* Blue background */
    colors['.']  = "\033[22;47m";     /* White stars */
    colors['\''] = "\033[2;40m";    /* Black border */
    colors['@']  = "\033[22;47m";     /* Tan poptart */
    colors['$']  = "\033[2;45m";     /* Pink poptart */
    colors['-']  = "\033[2;41m";     /* Red poptart */
    colors['>']  = "\033[2;41m";     /* Red rainbow */
    colors['&']  = "\033[2;43m";    /* Orange rainbow */
    colors['+']  = "\033[22;43m";     /* Yellow Rainbow */
    colors['#']  = "\033[2;42m";     /* Green rainbow */
    colors['=']  = "\033[22;46m";    /* Light blue rainbow */
    colors[';']  = "\033[2;46m";     /* Dark blue rainbow */
    colors['*']  = "\033[22;40m";     /* Gray cat face */
    colors['%']  = "\033[22;45m";     /* Pink cheeks */




    min_col = (FRAME_WIDTH - ws.ws_col / 2) / 2;
    max_col = (FRAME_WIDTH + ws.ws_col / 2) / 2;
    min_row = (FRAME_HEIGHT - ws.ws_row - 1) / 2;
    max_row = (FRAME_HEIGHT + ws.ws_row - 1) / 2;

    int i = 0;
    int l = 0;
    int color = 0;
    const char* output = "  ";

    time_t tm_start, tm_current;
    time(&tm_start);

    fprintf(stderr, "\e[2J\e[H");


    
    do {
        fprintf(stdout, "\e[H");

        int x, y;
        for(y = min_row; y < max_row; y++) {
            for(x = min_col; x < max_col; x++) {
                if(y > 23 && y < 43 && x < 0) {

                    int mod_x = ((-x + 2) % 16) / 8;
                    if((i / 2) % 2)
                        mod_x = 1 - mod_x;

                    const char* rainbow = ",,>>&&&+++###==;;;,,";
                    color = rainbow[mod_x + y - 23];

                    if(color == 0)
                        color = ',';
                
                } else if(x < 0 || y < 0 || y >= FRAME_HEIGHT || x >= FRAME_WIDTH)
                    color = ',';
                else
                    color = frames[i][y][x];

                if(color != l && colors[color]) {
                    l = color;
                    fprintf(stdout, "%s%s", colors[color], output);
                } else
                    fprintf(stdout, "%s", output);
            }

#if !defined(__aplus__)
            fprintf(stdout, "\n");
#endif
        }

        time(&tm_current);
        double diff = difftime(tm_current, tm_start);

        fprintf(stdout, "\033[3A");

        int j;
        for(j = (ws.ws_col - 29 - digits((int) diff)) / 2; j > 0; j--)
            fprintf(stdout, " ");

        fprintf(stdout, "\033[22;37mYou have nyaned for %0.0f seconds!\033[2;39m", diff);
        fflush(stdout);

        l = 0;
        i += 1;

        if(!frames[i])
            i = 0;

        usleep(50000);
    } while(1);

    return 0;
}
    