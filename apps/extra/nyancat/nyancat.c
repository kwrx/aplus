#include <stdio.h>
#include <unistd.h>
#include "animation.h"

#define TERM_W          80
#define TERM_H          24
#define FRAME_W         64
#define FRAME_H         64


#define clrscr()        fprintf(stderr, "\e[J")
#define lckscr()        putc('\4', stderr)
#define uckscr()        putc('\2', stderr)


static char* colors[256] = { 0 };


int main(int argc, char** argv) {
    clrscr();

    colors[',']  = "\033[0;34;44m";  /* Blue background */
	colors['.']  = "\033[1;37;47m";  /* White stars */
	colors['\''] = "\033[0;30;40m";  /* Black border */
	colors['@']  = "\033[1;37;47m";  /* Tan poptart */
	colors['$']  = "\033[1;35;45m";  /* Pink poptart */
	colors['-']  = "\033[1;31;41m";  /* Red poptart */
	colors['>']  = "\033[1;31;41m";  /* Red rainbow */
	colors['&']  = "\033[0;33;43m";  /* Orange rainbow */
	colors['+']  = "\033[1;33;43m";  /* Yellow Rainbow */
	colors['#']  = "\033[1;32;42m";  /* Green rainbow */
	colors['=']  = "\033[1;34;44m";  /* Light blue rainbow */
	colors[';']  = "\033[0;34;44m";  /* Dark blue rainbow */
	colors['*']  = "\033[1;30;40m";  /* Gray cat face */
	colors['%']  = "\033[1;35;45m";  /* Pink cheeks */

    do {
        int i;
        for(i = 0; frames[i]; i++) {
            lckscr();
            clrscr();

            int x, y;
            for(y = 0; y < TERM_H; y++) {
                for(x = 0; x < FRAME_W; x++) {
                    fprintf(stderr, "%s ", colors[frames[i][(FRAME_H / 2) - (TERM_H / 2) + y][x]]);
                }
                putc('\n', stderr);
            }

            uckscr();
            usleep(50000);
        }
    } while(1);

    return 0;
}
