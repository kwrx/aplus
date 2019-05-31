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
#include <string.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <libgen.h>
#include <time.h>


static void update_values();
static void dump_values();


static void show_usage(int argc, char** argv) {
    printf(
        "Use: taskman [OPTIONS...]\n"
        "Task Manager.\n\n"
        "   -h, --help                  show this help\n"
        "   -v, --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus systemutils) 0.1\n"
        "Copyright (c) %s Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], COMMIT, __DATE__ + 7, __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}


static void onexit_handler() {
    fprintf(stdout, "\e[49;39m");
    fflush(stdout);
}

static void timer_handler(int sig) {
    signal(sig, timer_handler);
    alarm(1);


    update_values();
    dump_values();
}


int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "hv", long_options, &idx)) != -1) {
        switch(c) {
            case 'v':
                show_version(argc, argv);
                break;
            case 'h':
            case '?':
                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }

    atexit(onexit_handler);
    signal(SIGALRM, timer_handler);
    signal(SIGCONT, timer_handler);
    alarm(1);


    update_values();
    dump_values();

    for(;;)
        pause();
}



typedef struct task {
    char user[16];
    pid_t pid;
    char name[64];
    char status[2];
    pid_t ppid;
    gid_t gid;
    uid_t sid;
    time_t utime;
    time_t stime;
    time_t cutime;
    time_t cstime;
    double cpu;
    int nice;
    int vmsize;
    int io;
    int iodiff;

    int unused;
    struct task* next;
} task_t;

static task_t* task_queue = NULL;


static void update_value(task_t* tq, task_t* tmp) {
    double tcpu = tmp->utime + tmp->stime;
    double qcpu = tq->utime + tq->stime;
    int tio = tmp->io;
    int qio = tq->io;

    memcpy(tq, tmp, sizeof(task_t) - sizeof(struct task*));
    tq->cpu = (tcpu - qcpu) / (CLOCKS_PER_SEC / 100.0);
    tq->io = tio;
    tq->iodiff = (tio - qio);
}

static void update_values() {
    
    DIR* d = opendir("/proc");
    if(!d) {
        perror("/proc");
        exit(-1);
    }

    struct dirent* ent;
    while((ent = readdir(d))) {
        if(!isdigit(ent->d_name[0]))
            continue;

        char buf[32];
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "/proc/%s/stat", ent->d_name);

        FILE* fp = fopen(buf, "rb");
        if(!fp) {
            perror(buf);
            exit(-1);
        }


        static task_t tmp;
        memset(&tmp, 0, sizeof(tmp));

        fscanf(fp, "%d", &tmp.pid);
        fscanf(fp, "%s", &tmp.name);
        fscanf(fp, "%s", &tmp.status);
        fscanf(fp, "%d", &tmp.ppid);
        fscanf(fp, "%d", &tmp.gid);
        fscanf(fp, "%d", &tmp.sid);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.utime);
        fscanf(fp, "%d", &tmp.stime);
        fscanf(fp, "%d", &tmp.cutime);
        fscanf(fp, "%d", &tmp.cstime);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.nice);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.vmsize);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fscanf(fp, "%d", &tmp.unused);
        fclose(fp);

        memset(buf, 0, sizeof(buf));
        sprintf(buf, "/proc/%s/io", ent->d_name);

        fp = fopen(buf, "rb");
        if(!fp) {
            perror(buf);
            exit(-1);
        }

        tmp.io = 0;
        
        int i = 0;
        static char buf2[BUFSIZ];
        while(fgets(buf2, BUFSIZ, fp) > 0) {
            if(i++ < 4)
                continue;

            char* p;
            if((p = strchr(buf2, ':')))
                tmp.io += atoi(++p);
        }

        fclose(fp);

        memmove(&tmp.name[0], &tmp.name[1], strlen(tmp.name));
        tmp.name[strlen(tmp.name) - 1] = 0;

        struct passwd* pwd;
        if((pwd = getpwuid(tmp.gid)) != NULL)
            strncpy(tmp.user, pwd->pw_name, 16);

        task_t* tq;
        for(tq = task_queue; tq; tq = tq->next) {
            if(tq->pid != tmp.pid)
                continue;

            break;
        }

        if(!tq) {
            tq = (task_t*) calloc(1, sizeof(task_t));
            tq->next = task_queue;
            task_queue = tq;
        }
        
        update_value(tq, &tmp);
    }

    closedir(d);
}

static void dump_values() {
    fprintf(stdout, "\e[2J\e[H\e[30;47m\e[2K");
    fprintf(stdout, " Pid  Name                 S  Priority    PPid     CPU        I/O       Memory\n");
    fprintf(stdout, "\e[0;39;49m");

    double cpu = 0;
    int io = 0;
    int memory = 0;
    
    task_t* tq;
    for(tq = task_queue; tq; tq = tq->next) {
        if(tq->status[0] == 'X' || tq->status[0] == 'Z')
            continue;

        if(tq->status[0] == 'R')
            fprintf(stdout, "\033[37m");

        if(tq->pid != 1)
            cpu += tq->cpu;
        
        io += tq->iodiff;
        memory += tq->vmsize;

        fprintf(stdout, " %-4d %-20s %-2s %-9d %6d %6.01f%% %6.02f MB/s %8.02f MB\n", tq->pid, basename(tq->name), tq->status, tq->nice, tq->ppid, tq->cpu, (double) tq->iodiff / 1024.0 / 1024.0, (double) tq->vmsize / 1024.0 / 1024.0);
        fprintf(stdout, "\033[39m");
    }

    fprintf(stdout, "\n\e[30;47m\e[2K");
    fprintf(stdout, " %-4s %-20s %-2s %-9s %6s %6.01f%% %6.02f MB/s %8.02f MB\n", "", "Total", "", "", "", cpu, (double) io / 1024.0 / 1024.0, (double) memory / 1024.0 / 1024.0);
    fprintf(stdout, "\033[0;39;49m");
    fflush(stdout);
}
