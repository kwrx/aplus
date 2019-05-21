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
#include <time.h>
#include <sys/times.h>
#include <unistd.h>
#include <errno.h>

unsigned int sleep(unsigned int seconds) {
    struct timespec tq, tr;
    tq.tv_sec = seconds;
    tq.tv_nsec = 0;
    
    if(nanosleep(&tq, &tr) == 0)
        return 0;
        
    if(errno == EINTR)
        return tr.tv_sec;
        
    return -1;
}
