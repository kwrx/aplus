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
void sh_jobs_signal(void);
