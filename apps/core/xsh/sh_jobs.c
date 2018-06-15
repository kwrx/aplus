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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/wait.h>

#include <aplus/base.h>
#include <aplus/utils/list.h>

#include "sh.h"

list(job_t*, sh_jobs);
job_t* background_job = NULL;

static job_t* jobs_find(int jobid) {
    job_t* job = NULL;
    list_each(sh_jobs, j) {
        if(j->id != jobid)
            continue;
        
        job = j;
        break;
    }

    if(!job) {
        errno = ENOENT;
        return NULL;
    }

    return job;
}

static job_t* jobs_find_by_pid(pid_t pid) {
    job_t* job = NULL;
    list_each(sh_jobs, j) {
        if(j->pid != pid)
            continue;
        
        job = j;
        break;
    }

    if(!job) {
        errno = ENOENT;
        return NULL;
    }

    return job;
}



static int jobs_wait(job_t* job) {
    int e, s;
    do {
        e = waitpid(job->pid, &s, 0);
    } while ((e == -1 && errno == EINTR));
    
    if(e == -1)
        return -1;


    if(WIFSTOPPED(s)) {
        fprintf(stdout, "[%d]%c Stopped\t%s\n", job->id, job->id == 0 ? '+' : '-', job->name);
        return 0;
    }

    list_remove(sh_jobs, job);
    free(job);
    return 0;
}


static void SIGCHLD_handler(int sig) {
    jobs_wait(background_job);
    background_job = NULL;
}

int sh_jobs_foreground(int jobid) {
    job_t* job;
    if(!(job = jobs_find(jobid)))
        return -1;

    if(job->status == JOB_STATUS_STOPPED)
        if(kill(job->pid, SIGCONT) != 0)
            return -1;


    sh_prepare_tty(job->pid);
    int e = jobs_wait(job);
    sh_reset_tty();

    return e;
}

int sh_jobs_background(int jobid) {
    job_t* job;
    if(!(job = jobs_find(jobid)))
        return -1;

    sh_jobs_signal(job);
    if(job->status == JOB_STATUS_STOPPED)
        if(kill(job->pid, SIGCONT) != 0)
            return -1;

    return 0;
}

int sh_jobs_new(pid_t pid, char* name, int stopped) {
    static int nextid = 0;

    job_t* job = (job_t*) calloc(1, sizeof(job_t));
    if(!job)
        return -1;

    if(list_length(sh_jobs) == 0)
        nextid = 0;

    job->id = nextid++;
    job->pid = pid;
    job->status = stopped ? JOB_STATUS_STOPPED : JOB_STATUS_RUNNING;
    strcpy(job->name, name);


    if(stopped)
        fprintf(stdout, "[%d]%c Stopped\t%s\n", job->id, job->id == 0 ? '+' : '-', job->name);
    
    list_push(sh_jobs, job);
    return job->id;
}

void sh_jobs_signal(job_t* job) {
    background_job = job;
    signal(SIGCHLD, SIGCHLD_handler);
}