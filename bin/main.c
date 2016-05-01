#include <stdio.h>
#include <signal.h>


int main(int argc, char** argv, char** env) {
	write(1, "Hello\n", 6);
	return 0;
}




extern void exit(int);

extern char** environ;
extern int __bss_start;
extern int end;

extern int __sigtramp(int);
extern void __libc_init_array();
extern void __libc_fini_array();

extern char** __get_envp();
extern char** __get_argv();
extern void __install_sighandler(void*);



void _init() {}
void _fini() {}


static int __default_sighandler__(int sig) {
	signal(sig, __default_sighandler__);
	_exit(sig);
}

static void __init_traps() {
	signal(SIGABRT, __default_sighandler__);
	signal(SIGFPE, __default_sighandler__);
	signal(SIGILL, __default_sighandler__);
	signal(SIGINT, __default_sighandler__);
	signal(SIGSEGV, __default_sighandler__);
	signal(SIGTERM, __default_sighandler__);
}


void _start(char** argv, char** environ) {
	long i;
	for(i = (long) &__bss_start; i < (long) &end; i++)
		*(unsigned char*) i = 0;


	int argc = 0;
	if(argv)
		while(argv[argc])
			argc++;

	__libc_init_array();

	__install_sighandler(__sigtramp);
	_init_signal();
	__init_traps();

	int ret = main(argc, argv, environ);

	__libc_fini_array();
	exit(ret);
}
