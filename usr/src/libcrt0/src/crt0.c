#include "syscalls.c"
#include <stdlib.h>
#include <signal.h>



extern char** environ;
extern int main(int, char**, char**);

extern int __bss_start;
extern int _end;

extern int __sigtramp(int sig);



static int __default_sighandler__(int sig) {
	signal(sig, __default_sighandler__);
	exit(sig);
}


#if 0
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
#endif

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


	environ = (char**) aplus_envp();
	char** argv = (char**) aplus_argv();

	int argc = 0;
	while(argv[argc])
		argc += 1;


	open("/dev/stdin", 0, 0644);
	open("/dev/stdout", 0, 0644);
	open("/dev/stderr", 0, 0644);


	aplus_install_sighandler(__sigtramp);
	_init_signal();
	__init_traps();

#if 0
	atexit(__do_global_dtors);
	__do_global_ctors();
#endif

	exit(main(argc, argv, environ));
}
