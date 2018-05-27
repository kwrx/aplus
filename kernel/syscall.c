#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/intr.h>
#include <libc.h>

#if 0
#undef CONFIG_SYSCALL_DEBUG
#define CONFIG_SYSCALL_DEBUG 1
#endif


#define MAX_SYSCALL            1024

typedef long (*syscall_handler_t)
        (long, long, long, long, long);


static spinlock_t lck_syscall;
static syscall_handler_t __handlers[MAX_SYSCALL];
#if CONFIG_SYSCALL_DEBUG
static char* __handlers_name[MAX_SYSCALL];
#endif

extern int syscalls_start;
extern int syscalls_end;




int syscall_init(void) {
    spinlock_init(&lck_syscall);

    memset(__handlers, 0, sizeof(__handlers));
    
    struct {
        int number;
        void* ptr;
        char* name;
    } *handler = (void*) &syscalls_start;

    for(
        handler = (void*) &syscalls_start;
        (uintptr_t) handler < (uintptr_t) &syscalls_end;
        handler++
    ) {
        syscall_register(handler->number, handler->ptr);
#if CONFIG_SYSCALL_DEBUG
        __handlers_name[handler->number] = handler->name;
#endif
    }


    return 0;
}


int syscall_register(int number, void* handler) {
    KASSERT(number < MAX_SYSCALL);
    KASSERTF(!__handlers[number], "%d", number);

    __handlers[number] = (syscall_handler_t) handler;
    return 0;
}

int syscall_unregister(int number) {
    KASSERT(number < MAX_SYSCALL);

    __handlers[number] = (syscall_handler_t) NULL;
    return 0;
}


long syscall_handler(long number, long p0, long p1, long p2, long p3, long p4) {
    KASSERTF(__handlers[number], "%d", number);
    
#if CONFIG_SYSCALL_DEBUG
    kprintf(LOG "syscall(%d): %s (%p, %p, %p, %p, %p) from %d\n", number, __handlers_name[number], p0, p1, p2, p3, p4, sys_getpid());
#endif

    //spinlock_lock(&lck_syscall);
    if(unlikely(spinlock_trylock(&lck_syscall) != 0))
        kprintf(WARN "syscall: context locked for %d from %d\n", number, sys_getpid());

    INTR_ON;
    long r = __handlers[number] (p0, p1, p2, p3, p4);
    syscall_ack();

    return r;
}

void syscall_ack(void) {
    INTR_ON;
    spinlock_unlock(&lck_syscall);
}

EXPORT(syscall_register);
EXPORT(syscall_unregister);
