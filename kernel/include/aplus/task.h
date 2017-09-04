#ifndef _TASK_H
#define _TASK_H

#include <aplus.h>
#include <aplus/base.h>
#include <aplus/mm.h>
#include <aplus/vfs.h>
#include <aplus/elf.h>
#include <aplus/utils/list.h>
#include <libc.h>


#define TASK_EXIT_EXITED            0
#define TASK_EXIT_STOPPED            1
#define TASK_EXIT_TERMED            2

#define TASK_STATUS_READY            0
#define TASK_STATUS_RUNNING            1
#define TASK_STATUS_SLEEP            2
#define TASK_STATUS_KILLED            3

#define TASK_PRIO_MAX                -20
#define TASK_PRIO_MIN                20
#define TASK_PRIO_REGULAR            0

#define TASK_FD_COUNT                32

#define TASK_ROOT_UID                0
#define TASK_ROOT_GID                0


#ifndef __ASSEMBLY__

typedef struct fd {
    inode_t* inode;
    int flags;
} fd_t;



typedef struct task {

    char* name;
    char* description;
    char** argv;
    char** environ;

    pid_t pid;
    uid_t uid;
    gid_t gid;
    uid_t sid;

    void* context;
    void* sys_stack;
        
    

    struct tms clock;
    time_t alarm;
    int status;
    int priority;
    uintptr_t vmsize;


    int (*sig_handler) (int);
    int16_t sig_no;
    uint64_t sig_mask;

    fifo_t fifo;
    fd_t fd[TASK_FD_COUNT];

    inode_t* root;
    inode_t* cwd;
    inode_t* exe;
    mode_t umask;


    list(struct task*, waiters);

    struct {
        int status:16;
        union {
            struct {
                int signo:8;
                int o177:8;
            } stopped;

            struct {
                int retval:8;
                int zero:8;
            } exited;

            struct {
                int zero:8;
                int corep:1;
                int signo:7;
            } termed;

            int value:16;
        };
    } exit;


    struct {
        uintptr_t start;
        uintptr_t end;
        symbol_t* symtab;
    } __image, *image;

    struct {
        uint64_t rchar;
        uint64_t wchar;
        uint64_t syscr;
        uint64_t syscw;
        uint64_t read_bytes;
        uint64_t write_bytes;
        uint64_t cancelled_write_bytes;
    } iostat;


    struct task* parent;
    struct task* next;
} task_t;


extern volatile task_t* current_task;
extern volatile task_t* kernel_task;
extern volatile task_t* task_queue;

void schedule(void);
void schedule_yield(void);
pid_t sched_nextpid();

extern void arch_task_switch(volatile task_t*, volatile task_t*);
extern volatile task_t* arch_task_fork(void);
extern volatile task_t* arch_task_clone(int (*) (void*), void*, int, void*);
extern void arch_task_yield(void);
extern void arch_task_release(volatile task_t* task);


#define task_create_tasklet(nm, handler, task)                                                                    \
        task = arch_task_clone(handler, NULL, CLONE_VM | CLONE_SIGHAND | CLONE_FILES | CLONE_FS, NULL);            \
        task->name = strdup(nm);




#endif

#endif
