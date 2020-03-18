#ifndef _APLUS_TASK_H
#define _APLUS_TASK_H

#ifndef __ASSEMBLY__

#define _GNU_SOURCE
#include <sched.h>

#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <sys/cdefs.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/syslimits.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>



#define TASK_STATUS_READY                       0
#define TASK_STATUS_RUNNING                     1
#define TASK_STATUS_SLEEP                       2
#define TASK_STATUS_STOP                        3
#define TASK_STATUS_ZOMBIE                      4


#define TASK_PRIO_MAX                           -20
#define TASK_PRIO_MIN                           19
#define TASK_PRIO_REG                           0


#define TASK_POLICY_RR                          0
#define TASK_POLICY_BATCH                       1
#define TASK_POLICY_IDLE                        2


#define TASK_CAPS_SYSTEM                        1
#define TASK_CAPS_IO                            2
#define TASK_CAPS_NETWORK                       4




struct fd {

    inode_t* inode;
    off_t position;

    struct {
        int flags:30;
        int close_on_exec:1;
    };

    spinlock_t lock;

};



typedef struct task {

    char** argv;
    char** environ;

    pid_t tid;
    gid_t tgid, pgid;
    uid_t uid, euid;
    gid_t gid, egid;
    uid_t sid;

    int status;
    int policy;
    int priority;
    int affinity;
    int caps;


    void* frame;
    vmm_address_space_t* address_space;


    struct tms clock;
    struct timespec sleep;
    struct fd fd[OPEN_MAX];

    struct {
        // TODO: Signal Handlers
        sigset_t mask;
    } signal;


    inode_t* root;
    inode_t* cwd;
    inode_t* exe;

    mode_t umask;


    struct {

        uintptr_t stack;
        uintptr_t sigstack;
        uintptr_t start;
        uintptr_t end;

    } userspace;


    struct {
        union {
            struct {
                int16_t o177:8;
                int16_t signo:8;
            } stopped;

            struct {
                int16_t zero:8;
                int16_t retval:8;
            } exited;

            struct {
                int16_t signo:7;
                int16_t corep:1;
                int16_t zero:8;
            } termed;
        };

        int16_t value:16;
    } exit;


    struct {
        uint64_t rchar;
        uint64_t wchar;
        uint64_t syscr;
        uint64_t syscw;
        uint64_t read_bytes;
        uint64_t write_bytes;
        uint64_t cancelled_write_bytes;     
    } iostat;


    struct rlimit rlimits[RLIM_NLIMITS];
    struct rusage rusage;

    spinlock_t lock;



    struct task* parent;
    struct task* next;

} task_t;



__BEGIN_DECLS


pid_t sched_nextpid();

void sched_enqueue(task_t*);
void sched_dequeue(task_t*);

void schedule(int);


extern task_t* sched_queue;

__END_DECLS

#endif
#endif