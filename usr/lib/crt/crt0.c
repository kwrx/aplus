
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


#include <fcntl.h>
#include <string.h>
#include "syscalls.c"


extern char** environ;

static void __signal_handler__(int sig) {
	perror(strsignal(sig));
	raise(sig);
}

static void __open_stdio__() {
	open("/dev/stdin", O_RDWR, 0644);
	open("/dev/stdout", O_RDWR, 0644);
	open("/dev/stderr", O_RDWR, 0644);
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

	__install_signal_handler(__signal_handler__);
	_init_signal();

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

	_exit(retcode);
}

#ifdef __cplusplus
}
#endif
