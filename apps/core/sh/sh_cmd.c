#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <aplus/base.h>
#include <aplus/utils/list.h>

#include "sh.h"


int sh_cmd_jobs(int argc, char** argv) {
    list_each(sh_jobs, job) {
        fprintf(stdout, 
            "[%d]%c %s\t%s\n",
            job->id,
            job->id == 0 ? '+' : '-',
            job->status == JOB_STATUS_RUNNING ? "Running" : "Stopped",
            job->name
        );
    }

    return 0;
}

int sh_cmd_fg(int argc, char** argv) {
    int e;
    e = (argc < 2) 
        ? sh_jobs_foreground(0)
        : sh_jobs_foreground(atoi(argv[1]));

    if(e < 0)
        perror("sh: fg");
    
    return e;
}

int sh_cmd_bg(int argc, char** argv) {
    int e;
    e = argc < 2 
        ? sh_jobs_background(0)
        : sh_jobs_background(atoi(argv[1]));

    if(e < 0)
        perror("sh: bg");
    
    return e;
}


int sh_cmd_pwd(int argc, char** argv) {
    char buf[BUFSIZ];
    fprintf(stdout, "%s\n", getcwd(buf, BUFSIZ));

    return 0;
}

int sh_cmd_cd(int argc, char** argv) {
    if(!argv[1])
        return -1;

    if(chdir(argv[1]) != 0) {
        perror(argv[1]);
        return -1;
    }

    char buf[BUFSIZ];
    setenv("PWD", getcwd(buf, BUFSIZ), 1);

    return 0;
}

int sh_cmd_exit(int argc, char** argv) {
    list_each(sh_jobs, job)
        kill(job->pid, SIGKILL);

    if(argv[1])
        exit(atoi(argv[1]));

    exit(0);
}

int sh_cmd_export(int argc, char** argv) {
    if(argc < 2) {
        int i;
        for(i = 0; environ[i]; i++)
            fprintf(stdout, "%s\n", environ[i]);

        return 0;
    }

    int e;
    if(strchr(argv[1], '='))
        e = putenv(argv[1]);
    else
        e = setenv("_", argv[1], 1);


    if(e == -1)
        perror(argv[1]);

    return e;
}

int sh_cmd_history(int argc, char** argv) {
    int i = 0;
    list_each(sh_history, h)
        fprintf(stdout, "%-6d %s\n", i++, h);

    return 0;
}

struct sh_command_list sh_commands[] = {
    { "jobs", "Display status of jobs", sh_cmd_jobs },
    { "fg", "Move jobs to the foreground", sh_cmd_fg },
    { "bg", "Move jobs into the background", sh_cmd_bg },
    { "pwd", "Print the name of the current working directory", sh_cmd_pwd },
    { "cd", "Change current working directory", sh_cmd_cd },
    { "exit", "Exit the shell", sh_cmd_exit },
    { "export", "Set environment variable", sh_cmd_export },
    { "history", "List command history", sh_cmd_history },
    { NULL, NULL, NULL }
};


