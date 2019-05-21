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


#pragma once

#include <aplus/base.h>
#include <aplus/utils/list.h>

#define JOB_STATUS_STOPPED          0
#define JOB_STATUS_RUNNING          1



typedef int (*sh_command_t) (int, char**);

struct sh_command_list {
    char* cmd;
    char* description;
    sh_command_t fn;
};

typedef struct {
    int id;
    pid_t pid;
    int status;
    char name[64];
} job_t;


extern list(char*, sh_history);
extern list(job_t*, sh_jobs);
extern struct sh_command_list sh_commands[];

char* sh_prompt(char* line, char* user, char* host, int last_err);
void sh_history_add(char* line);
int sh_exec(char*);

void sh_reset_tty(void);
void sh_prepare_tty(pid_t pgrp);

int sh_jobs_new(pid_t pid, char* name, int stopped);
int sh_jobs_background(int jobid);
int sh_jobs_foreground(int jobid);
void sh_jobs_signal(job_t*);
