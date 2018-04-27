#ifndef _IPC_H
#define _IPC_H


#define MTX_KIND_DEFAULT                0
#define MTX_KIND_ERRORCHECK             1
#define MTX_KIND_RECURSIVE              2

#define SPINLOCK_LOCKED                 1
#define SPINLOCK_UNLOCKED               0


#ifndef __ASSEMBLY__

#define MTX_INIT(t, n)                  \
    {                                   \
        0,                              \
        0,                              \
        t,                              \
        -1,                             \
        n                               \
    }

typedef volatile long spinlock_t;

typedef struct {
    spinlock_t lock;
    long recursion;
    long kind;
    long owner;
    const char* name;
} __packed mutex_t;


typedef struct fifo {
    uint8_t buffer[BUFSIZ];
    int w_pos;
    int r_pos;
    mutex_t w_lock;
    mutex_t r_lock;
} __packed fifo_t;





#if CONFIG_IPC
int spinlock_init(spinlock_t* lock);
void spinlock_lock(spinlock_t* lock);
int spinlock_trylock(spinlock_t* lock);
void spinlock_unlock(spinlock_t* lock);

int mutex_init(mutex_t* mtx, long kind, const char* name);
int mutex_lock(mutex_t* mtx);
int mutex_trylock(mutex_t* mtx);
int mutex_unlock(mutex_t* mtx);
#elif !CONFIG_IPC
#    define spinlock_init(x) 
#    define spinlock_lock(x)                ((void) x)
#    define spinlock_trylock(x)             (1)
#    define spinlock_unlock(x)              ((void) x)

#    define mutex_init(x, y, z) 
#    define mutex_lock(x)                   ((void) x)
#    define mutex_trylock(x)                (1)
#    define mutex_unlock(x)                 ((void) x)
#endif


int fifo_read(fifo_t* fifo, void* ptr, size_t len);
int fifo_write(fifo_t* fifo, void* ptr, size_t len);
int fifo_available(fifo_t* fifo);

#define fifo_init(x)                                                    \
    {                                                                   \
        (x)->w_pos = (x)->r_pos = 0;                                    \
        mutex_init(&(x)->r_lock, MTX_KIND_DEFAULT, "fifo");             \
        mutex_init(&(x)->w_lock, MTX_KIND_DEFAULT, "fifo");             \
    }



#define ipc_timed_wait(cond, tm)                                        \
    {                                                                   \
        ktime_t __t = timer_getms() + tm;                               \
        while((cond)) {                                                 \
            if(timer_getms() > __t) {                                   \
                kprintf(WARN "ipc: deadlock timeout expired for "       \
                             "(%s) %s:%d!\n",                           \
                             #cond, __FILE__, __LINE__);                \
                debug_stacktrace(5);                                    \
                                                                        \
                __t = timer_getms() + tm;                               \
            }                                                           \
                                                                        \
            sys_yield();                                                \
        }                                                               \
    }    

#if DEBUG
#define ipc_wait(cond)                                                  \
    {                                                                   \
        ktime_t __t = timer_getms() + CONFIG_IPC_TIMEOUT;               \
        while((cond)) {                                                 \
            if(timer_getms() > __t) {                                   \
                kprintf(WARN "ipc: deadlock timeout expired for "       \
                             "(%s) %s:%d!\n",                           \
                             #cond, __FILE__, __LINE__);                \
                debug_stacktrace(5);                                    \
                                                                        \
                __t = timer_getms() + CONFIG_IPC_TIMEOUT;               \
            }                                                           \
                                                                        \
            sys_yield();                                                \
        }                                                               \
    }     
#else
#define ipc_wait(cond)                                                  \
    {                                                                   \
        while((cond))                                                   \
            sys_yield();                                                \
    }   
#endif

#endif
#endif
