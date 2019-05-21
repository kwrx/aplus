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
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <termios.h>


int main(int argc, char** argv) {
    struct termios ios;
    ioctl(STDOUT_FILENO, TCGETA, &ios);

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

    ios.__c_ispeed =
    ios.__c_ospeed = B9600;

    ioctl(STDOUT_FILENO, TCSETA, &ios);


    fprintf(stdout, "\e[0;39;49m");
    fprintf(stdout, "\e[2J\e[H");
    fflush(stdout);
    return 0;
}