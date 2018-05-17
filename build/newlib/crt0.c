#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

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


static void __init_tz() {
    int fd = open("/etc/localtime", O_RDONLY);
    if(fd < 0)
        return;
    
    struct {
        char magic[4];
        char version;
    } head;

    if(read(fd, &head, sizeof(head)) != sizeof(head))
        goto done;

    if(strncmp(head.magic, "TZif", 4) != 0)
        goto done;

    if(head.version < '2')
        goto done;

    lseek(fd, -2, SEEK_END);
    do {
        char ch;
        if(read(fd, &ch, 1) != 1)
            break;

        if(ch == '\n') {
            char buf[64];
            memset(buf, 0, sizeof(buf));

            if(read(fd, buf, sizeof(buf)) <= 0)
                break;
            
            buf[strlen(buf) - 1] = '\0';
            setenv("TZ", buf, 1);
            break;
        }
    } while(lseek(fd, -2, SEEK_CUR) > 44);

done:
    close(fd);
}

void _start(char** argv, char** env) {
    long i;
    for(i = (long) &__bss_start; i < (long) &end; i++)
        *(unsigned char*) i = 0;

    
    __libc_init_array();
    __install_sighandler(__sigtramp_handler);
    __init_traps();


    char* p;
    if(__builtin_expect(argv && argv[0], 1))
        strncpy(__progname, (p = strrchr(argv[0], '/'))
                                ? p + 1
                                : argv[0], BUFSIZ);

    int argc = 0;
    if(__builtin_expect(argv, 1))
        while(argv[argc])
            argc++;

    if(__builtin_expect(env, 1))
        while(*env)
            putenv(*env++);

    __init_tz();
    tzset();

    atexit(__libc_fini_array);
    exit(main(argc, argv, environ) & 0377);
}
