#include "syscalls.c"
#include <stdlib.h>
#include <signal.h>


#ifndef F_DUPFD
#define F_DUPFD			0
#endif


extern char** environ;
extern int main(int, char**, char**);

extern int __bss_start;
extern int _end;

extern int __sigtramp(int sig);
extern void __libc_init_array();
extern void __libc_fini_array();

void _init() {
	fcntl(open("/dev/stdin", 0, 0644), F_DUPFD, 0);
	fcntl(open("/dev/stdout", 0, 0644), F_DUPFD, 1);
	fcntl(open("/dev/stderr", 0, 0644), F_DUPFD, 2);
}

void _fini() {
	
}

static int __default_sighandler__(int sig) {
	signal(sig, __default_sighandler__);
	_exit(sig);
}


/*
static void __do_global_ctors() {
	extern int __ctors_start;
	extern int __ctors_end;

	void (**fs) () = (void (**) ()) &__ctors_start;
	void (**fe) () = (void (**) ()) &__ctors_end;
	
	while(fs < fe) {
		(*fs) ();
		fs++;
	}
}

static void __do_global_dtors() {
	extern int __dtors_start;
	extern int __dtors_end;

	void (*fs) () = (void (*) ()) &__dtors_start;
	void (*fe) () = (void (*) ()) &__dtors_end;
	
	while(fs < fe) {
		(*fs) ();
		fs++;
	}
}
*/


static void __init_traps() {
	signal(SIGABRT, __default_sighandler__);
	signal(SIGFPE, __default_sighandler__);
	signal(SIGILL, __default_sighandler__);
	signal(SIGINT, __default_sighandler__);
	signal(SIGSEGV, __default_sighandler__);
	signal(SIGTERM, __default_sighandler__);
}


__attribute__((noreturn))
void _start() {
	
	int i;
	for(i = (int) &__bss_start; i < (int) &_end; i++)
		*(char*) i = 0;


	environ = (char**) __get_envp();
	char** argv = (char**) __get_argv();

	int argc = 0;

	if(argv)
		while(argv[argc])
			argc += 1;


	__libc_init_array();

	__install_sighandler(__sigtramp);
	_init_signal();
	__init_traps();

#if 0
	//atexit(__do_global_dtors);
	__do_global_ctors();
#endif

	int ret = main(argc, argv, environ);

	__libc_fini_array();
	exit(ret);
}
