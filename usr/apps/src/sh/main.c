//
//  main.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include <sys/times.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#include <aplus/events.h>
#include <aplus/ioctl.h>

#include "sh.h"

static int do_command(char** argv) {
	if(argv[0] == NULL || *argv[0] == NULL)
		return -1;
		
	if(strcmp(argv[0], "cd") == 0)
		return chdir(argv[1]);
		
	if(strcmp(argv[0], "exit") == 0)
		exit(0);
		
	int retcode = -1;
	if(fork() == 0)
		_exit(execvp(argv[0], argv));
	else
		wait(&retcode);
		
	return retcode;
}

static int do_input(char* input) {
	char** argv = malloc(1024);
	char* ip = strdup(input);
	char* sp = ip;
	int cnt = 0;
	
	while(*sp) {
		if(sp = strchr(sp, ' ')) {
			*sp++ = 0;
			argv[cnt++] = ip;
			
			ip = sp;
		} else {
			argv[cnt++] = ip;
			break;
		}
	}

	return do_command(argv);
}



int main(int argc, char** argv) {
	if(argc > 1) {
		if(strcmp(argv[1], "-c") == 0)
			exit(do_command(&argv[2]));
	
		for(int i = 0; i < argc; i++) {
			if(strcmp(argv[i], "--help") == 0)
				exit(show_help());
			
			if(strcmp(argv[i], "--version") == 0)
				exit(show_version());
		}
		
		exit(show_help());
	}
	
	
	char* username = getenv("USER");
	char* currentdir = malloc(BUFSIZ);
	char* input = malloc(BUFSIZ);
	
	for(;;) {
		getcwd(currentdir, 1024);
		dprintf(1, "%s:%s", username, currentdir);
		dprintf(1, "$ ");
		
		
		memset(input, 0, BUFSIZ);
		
		
		if(gets(input))
			if(strlen(input) > 0)
				if(do_input(input) != 0)
					perror(SH_NAME);
		
	}
}