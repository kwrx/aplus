#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>


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
        "Copyright (c) 2016-2017 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}


static void onexit_handler() {
    fprintf(stdout, "\e[49;39m");
    fflush(stdout);
}

static void timer_handler(int sig) {
    signal(SIGALRM, timer_handler);
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
    alarm(1);


    update_values();
    dump_values();

    char ch;
    while(read(STDIN_FILENO, &ch, 1))
        ;
}



typedef struct task {
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
    int cpu;
    int nice;
    int vmsize;
    int io;

    int unused;
    struct task* next;
} task_t;

static task_t* task_queue = NULL;


static void update_value(task_t* tq, task_t* tmp) {
    int tcpu = tmp->utime + tmp->stime;
    int qcpu = tq->utime + tq->stime;
    int tio = tmp->io;
    int qio = tq->io;

    memcpy(tq, tmp, sizeof(task_t) - sizeof(struct task*));
    tq->cpu = (tcpu - qcpu) / 10;
    tq->io = (tio - qio);
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
    fprintf(stdout, "\e[2J\e[30;47m\e[K");
    fprintf(stdout, " %-4s %-24s %-6s %6s %7s  %10s  %10s\n", "Pid", "Name", "Status", "PPid", "CPU", "I/O", "Memory");
    fprintf(stdout, "\e[39;49m");

    int cpu = 0;
    int io = 0;
    int memory = 0;
    
    task_t* tq;
    for(tq = task_queue; tq; tq = tq->next) {
        if(tq->status[0] == 'X')
            continue;

        if(tq->status[0] == 'R')
            fprintf(stdout, "\033[37m");

        cpu += tq->cpu;
        io += tq->io;
        memory += tq->vmsize;

        fprintf(stdout, " %-4d %-24s %-6s %6d %6d%% %6.02g MB/s %8.02g MB\n", tq->pid, tq->name, tq->status, tq->ppid, tq->cpu, (double) tq->io / 1024.0 / 1024.0, (double) tq->vmsize / 1024.0 / 1024.0);
        fprintf(stdout, "\033[39m");
    }

    fprintf(stdout, "\n\e[30;47m\e[K");
    fprintf(stdout, " %-4s %-24s %-6s %6s %6d%% %6.02g MB/s %8.02g MB\n", "", "Total", "", "", cpu, (double) io / 1024.0 / 1024.0, (double) memory / 1024.0 / 1024.0);
    fprintf(stdout, "\033[39;49m");
    fflush(stdout);
}
