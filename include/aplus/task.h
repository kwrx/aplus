#ifndef _APLUS_TASK_H
#define _APLUS_TASK_H

#ifndef __ASSEMBLY__

#define _GNU_SOURCE
#include <sched.h>

#include <stdint.h>
#include <signal.h>
#include <time.h>

#include <sys/times.h>
#include <sys/resource.h>

#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/smp.h>

#include <aplus/utils/list.h>



#define TASK_STATUS_READY                       0
#define TASK_STATUS_RUNNING                     1
#define TASK_STATUS_SLEEP                       2
#define TASK_STATUS_STOP                        3
#define TASK_STATUS_ZOMBIE                      4
#define TASK_STATUS_DEAD                        5


#define TASK_PRIO_MAX                           -20
#define TASK_PRIO_MIN                           19
#define TASK_PRIO_REG                           0


#define TASK_POLICY_RR                          0
#define TASK_POLICY_BATCH                       1
#define TASK_POLICY_IDLE                        2


#define TASK_FLAGS_NO_FRAME                     1
#define TASK_FLAGS_NO_FPU                       2
#define TASK_FLAGS_NEED_RESCHED                 4


#define TASK_CAPS_SYSTEM                        255
#define TASK_CAPS_IO                            2
#define TASK_CAPS_NETWORK                       4


#define TASK_CLOCK_MAX                          3
#define TASK_CLOCK_SCHEDULER                    0
#define TASK_CLOCK_THREAD_CPUTIME               1
#define TASK_CLOCK_PROCESS_CPUTIME              2


#define TASK_SCHEDULER_PERIOD_NS                5000000ULL

#define TASK_STACK_MAX                          (0x100000000ULL)    // 4GiB
#define TASK_STACK_MIN                          (0x1000ULL)         // 4KiB



struct fd {

    struct {
        
        struct file* ref;

        struct {
            int flags:30;
            int close_on_exec:1;
        };

    } descriptors[CONFIG_OPEN_MAX];

    size_t refcount;

};


struct fs {

    inode_t* root;
    inode_t* cwd;
    inode_t* exe;

    mode_t umask;
    size_t refcount;

};


struct ksigaction {
	
    union {
        void (*handler) (int);
        void (*sigaction) (int, siginfo_t*, void*);
    };

	long flags;
	void (*sa_restorer)(void);
	
    int mask[2];

};


struct kclone_args {
    
    uint64_t flags;         /* Flags bit mask                                           */
    uint64_t pidfd;         /* Where to store PID file descriptor (pid_t *)             */
    uint64_t child_tid;     /* Where to store child TID, in child's memory (pid_t *)    */
    uint64_t parent_tid;    /* Where to store child TID, in parent's memory (int *)     */
    uint64_t exit_signal;   /* Signal to deliver to parent on child termination         */
    uint64_t stack;         /* Pointer to lowest byte of stack                          */
    uint64_t stack_size;    /* Size of stack                                            */
    uint64_t tls;           /* Location of new TLS                                      */
    uint64_t set_tid;       /* Pointer to a pid_t array                                 */
    uint64_t set_tid_size;  /* Number of elements in set_tid                            */

};


struct sighand {

    struct ksigaction action[128];
    sigset_t sigmask;
    size_t refcount;

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
    int flags;
    int caps;


    void* frame;
    void* sp0;
    void* sp3;
    void* fpu;
    vmm_address_space_t* address_space;


    
    struct {

        clockid_t clockid;
        struct timespec timeout;
    
    } sleep;


    struct timespec clock[TASK_CLOCK_MAX];


    list(futex_t*, futexes);
    list(struct task*, wait_queue);

    int  wait_options;
    int* wait_status;
    struct rusage* wait_rusage;


    struct fd* fd;
    struct fs* fs;
    struct sighand* sighand;


    struct {

        uintptr_t stack;
        uintptr_t sigstack;
        uintptr_t start;
        uintptr_t end;

        uintptr_t thread_area;
        uintptr_t cpu_area;

        uintptr_t tid_address;

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
    spinlock_t sched_lock;

    struct task* parent;
    struct task* next;

} task_t;



#define thread_postpone_resched(task)       \
    task->flags |= TASK_FLAGS_NEED_RESCHED

#define thread_suspend(task)                \
    task->status = TASK_STATUS_SLEEP

#define thread_wake(task)                   \
    task->status = TASK_STATUS_READY

#define thread_wake_and_return(task, ret)                               \
    {                                                                   \
        arch_task_context_set(task, ARCH_TASK_CONTEXT_RETVAL, ret);     \
        thread_wake(task);                                              \
    }



__BEGIN_DECLS

struct cpu;

void do_unshare(int);
pid_t do_fork(struct kclone_args*, size_t);

pid_t sched_nextpid();
void sched_enqueue(task_t*);
void sched_dequeue(task_t*);

void schedule(int);

__END_DECLS

#endif
#endif