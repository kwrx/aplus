#include <signal.h>
#include <stdio.h>


int __aplus_crt__ = 1;

extern void exit(int);
extern int main(int, char**, char**);

extern char** environ;
extern int __bss_start;
extern int end;

extern int __sigtramp(int);
extern void __libc_init_array();
extern void __libc_fini_array();

extern void __install_sighandler(void*);




static void __default_sighandler__(int sig) {
	psignal(sig, "exception");
	signal(sig, __default_sighandler__);
	_exit((sig & 0177) << 8);
}

static void __default_sighandler__no_echo(int sig) {
    signal(sig, __default_sighandler__no_echo);
	_exit((sig & 0177) << 8);
}

static void __init_traps() {
	signal(SIGABRT, __default_sighandler__no_echo);
	signal(SIGFPE, __default_sighandler__);
	signal(SIGILL, __default_sighandler__);
	signal(SIGINT, __default_sighandler__);
	signal(SIGSEGV, __default_sighandler__);
	signal(SIGTERM, __default_sighandler__);
	signal(SIGQUIT, __default_sighandler__no_echo);
	signal(SIGKILL, __default_sighandler__no_echo);
}


void _start(char** argv, char** env) {
	long i;
	for(i = (long) &__bss_start; i < (long) &end; i++)
		*(unsigned char*) i = 0;


	int argc = 0;
	if(argv)
		while(argv[argc])
			argc++;
			
	environ = env;


	__libc_init_array();
	
	__install_sighandler(__sigtramp);
	_init_signal();
	__init_traps();
	
	atexit(__libc_fini_array);
	
	int ret = main(argc, argv, environ);


	//__libc_fini_array();
	exit(ret);
}
