#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/termio.h>
#include <sys/termios.h>


int main(int argc, char** argv) {
    struct termios ios;
    ioctl(STDOUT_FILENO, TIOCGETA, &ios);

    ios.c_iflag = 0;
    ios.c_oflag = 0;
    ios.c_cflag = 0;
    ios.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHONL;

    ios.c_cc[VEOF] = 000;
    ios.c_cc[VEOL] = 000;
    ios.c_cc[VERASE] = 0177;
    ios.c_cc[VINTR] = 003;
    ios.c_cc[VKILL] = 025;
    ios.c_cc[VQUIT] = 034;
    ios.c_cc[VSTART] = 002;
    ios.c_cc[VSTOP] = 004;
    ios.c_cc[VMIN] = 0;

    ios.c_ispeed =
    ios.c_ospeed = B9600;

    ioctl(STDOUT_FILENO, TIOCSETA, &ios);


    fprintf(stdout, "\e[0;39;49m");
    fprintf(stdout, "\e[2J\e[H");
    fflush(stdout);
    return 0;
}