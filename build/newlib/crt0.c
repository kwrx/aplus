#include <signal.h>
#include <stdio.h>
#include <string.h>

#ifdef _POSIX_SOURCE
#undef _POSIX_SOURCE
#endif
#include <sys/wait.h>

int __aplus_crt__ = 1;
char* __progname[BUFSIZ];

extern void exit(int);
extern int main(int, char**, char**);

extern char** environ;
extern int __bss_start;
extern int end;

extern int __sigtramp(int);
extern void __libc_init_array();
extern void __libc_fini_array();
extern void __install_sighandler(void*);



static void __df_sighandler_TERM(int sig) {
    _exit((1 << 31) | W_EXITCODE(0, sig));
}

static void __df_sighandler_CORE(int sig) {
    /* TODO: Core dumping */
    _exit((1 << 31) | W_EXITCODE(0, sig) | WCOREFLAG);
}

static void __df_sighandler_STOP(int sig) {
    _exit((1 << 31) | W_STOPCODE(sig));
}



static int __sigtramp_handler(int sig) {
    int e = __sigtramp(sig);
    switch(e) {
        case 2:
        case 3:
            break;
        case 1:
            switch(sig) {
                #define _(x, y) \
                    case x : { __df_sighandler_##y (sig); break; }
   
                _(SIGHUP,   TERM);
                _(SIGINT,   TERM);
                _(SIGQUIT,  CORE);
                _(SIGILL,   CORE);
                _(SIGABRT,  CORE);
                _(SIGFPE,   CORE);
                //_(SIGKILL, SIG_IGN);
                _(SIGSEGV,  CORE);
                _(SIGPIPE,  TERM);
                _(SIGALRM,  TERM);
                _(SIGTERM,  TERM);
                _(SIGUSR1,  TERM);
                _(SIGUSR2,  TERM);
                //_(SIGCHLD,  SIG_IGN);
                //_(SIGCONT,  CONT);
                //_(SIGSTOP,  SIG_IGN);
                _(SIGTSTP,  STOP);
                _(SIGTTIN,  STOP);
                _(SIGTTOU,  STOP);
                _(SIGBUS,   CORE);
                _(SIGPOLL,  TERM);
                _(SIGPROF,  TERM);
                _(SIGSYS,   CORE);
                _(SIGTRAP,  CORE);
                //_(SIGURG,   SIG_IGN);
                _(SIGVTALRM,TERM);
                _(SIGXCPU,  CORE);
                _(SIGXFSZ,  CORE);
                //_(SIGWINCH, SIG_IGN);

                #undef _

                default:
                    break;
            }
    }

    return 0;
}


static void __init_traps() {
    _init_signal();

    int i;
    for(i = 0; i < NSIG; i++)
        signal(i, SIG_DFL);
}


void _start(char** argv, char** env) {
    long i;
    for(i = (long) &__bss_start; i < (long) &end; i++)
        *(unsigned char*) i = 0;

    char* p;
    if(argv && argv[0])
        strncpy(__progname, (p = strrchr(argv[0], '/'))
                                ? p + 1
                                : argv[0], BUFSIZ);

    int argc = 0;
    if(argv)
        while(argv[argc])
            argc++;

    environ = env;


    __libc_init_array();

    __install_sighandler(__sigtramp_handler);
    __init_traps();

    atexit(__libc_fini_array);
    exit(main(argc, argv, environ) & 0377);
}
