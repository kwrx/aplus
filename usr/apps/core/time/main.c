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
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/times.h>
#include <sys/wait.h>



int main(int argc, char** argv) {
    
    void show_time(struct tms* s, struct tms* e, clock_t cs, clock_t ce) {
        printf(
            "real   %gs\n"
            "user   %gs\n"
            "sys    %gs\n",
            (float) (ce - cs) / CLOCKS_PER_SEC,
            (float) (e->tms_cutime - s->tms_cutime) / CLOCKS_PER_SEC,
            (float) (e->tms_cstime - s->tms_cstime) / CLOCKS_PER_SEC
        );
        
        exit(0);
    }
    
    struct tms ts, te;
    clock_t cs, ce;
    
    cs = times(&ts);
    
    if(argc < 2)
        show_time(&ts, &ts, cs, cs);
    
    
    int e = fork();
    if(e == 0)
        execvp(argv[1], &argv[1]);
    else if(e == -1) {
        perror("fork");
        exit(-1);
    } else {
        wait(NULL);
        
        ce = times(&te);
        show_time(&ts, &te, cs, ce);
    }
    
    
    return 0;
}