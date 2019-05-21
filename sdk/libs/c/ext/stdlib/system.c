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


#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>


int _system(const char* s) {
    char* argv[4];
    argv[0] = "/usr/bin/sh";
    argv[1] = "-c";
    argv[2] = (char*) s;
    argv[3] = NULL;

    int e = fork();
    if(e == 0) {
        execve(argv[0], argv, environ);
        exit(100);
    } else if(e == -1)
        return -1;

    int st;
    e = wait(&st);
    if(e == -1)
        return -1;

    st = (st >> 8) & 0xFF;
    return st;
}