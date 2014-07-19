
//
//  crt0.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is kfree software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the kfree Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.


#ifdef  __cplusplus
extern "C" {
#endif

void* __dso_handle = 0;


#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include "syscalls.c"


extern char** environ;
extern int __sigtramp(int sig);

static int __default_sighandler__(int sig) {
	signal(sig, __default_sighandler__);
	exit(sig);
}


static void __open_stdio__() {
	open("/dev/stdin", O_RDWR, 0644);
	open("/dev/stdout", O_RDWR, 0644);
	open("/dev/stderr", O_RDWR, 0644);
}


static void __init_traps() {
	signal(SIGABRT, __default_sighandler__);
	signal(SIGFPE, __default_sighandler__);
	signal(SIGILL, __default_sighandler__);
	signal(SIGINT, __default_sighandler__);
	signal(SIGSEGV, __default_sighandler__);
	signal(SIGTERM, __default_sighandler__);
}

void _start() {
	extern int main();
	extern void _init_signal();
	extern void __do_global_ctors_aux();
	extern void __do_global_dtors_aux();
	extern int __bss_start;
	extern int _end;

	int i;
	for(i = (int)&__bss_start; i < (int)&_end; i++)
		*(char*) i = 0;


	__open_stdio__();

	__install_signal_handler(__sigtramp);
	_init_signal();
	__init_traps();

	int argc;
	char** argv = __get_argv();
	char** env = __get_env();

	if(argv)
		while(argv[argc])
			argc++;

	environ = env;

	//__do_global_ctors_aux();
	int retcode = main(argc, argv, env);
	//__do_global_dtors_aux();

	exit(retcode);
}

#ifdef __cplusplus
}
#endif
