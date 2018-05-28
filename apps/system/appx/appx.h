/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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


#ifndef _APPX_H
#define _APPX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#include <aplus/base.h>
#include <zip.h>


#define APPX_LOCK \
    "/tmp/appx.lck"


extern int verbose;
extern int yes;

void appx_lck_acquire();
void appx_lck_release();
void appx_install_from(const char* filename);


#define breakpoint() {                                                                                                                  \
        fprintf(stderr, "breakpoint: file %s, line %d, function %s\nPress enter key to continue...\n", __FILE__, __LINE__, __func__);   \
        char ch;                                                                                                                        \
        fgets(&ch, 1, stdin);                                                                                                           \
    }

#endif