#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>


SYSCALL(90, sysconf,
long sys_sysconf(int n) {
    errno = 0;

	switch(n) {
        #define __case(x, y) case x: return y

        __case(_SC_ARG_MAX, 4096);
        __case(_SC_CHILD_MAX, -1);
        __case(_SC_NGROUPS_MAX, 1);
        __case(_SC_CLK_TCK, CLOCKS_PER_SEC);
        __case(_SC_OPEN_MAX, 20);
        __case(_SC_PAGESIZE, PAGE_SIZE);
        __case(_SC_JOB_CONTROL, -1);
        __case(_SC_SAVED_IDS, 0);
        __case(_SC_VERSION, 20170101L);
        
        default:
            errno = EINVAL;
            return -1;
    }
});
