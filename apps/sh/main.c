#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sh.h"
#include "../coreutils/coreutils.h"


struct command {
	char* name;
	int (*handler) (char** argv);
} commands[3] = {
	{ "cd", cmd_cd },
	{ "exit", cmd_exit },
	{ NULL, NULL }
};


static void die(int e) {
	printf("%s: %s\n", __PROGNAME, __errors[e]);
	exit(1);
}

static void reset() {
	stdin = freopen("/dev/kbd", "r+", stdin);
	if(!stdin)
		die(ERR_KBD);
}


static void do_exec(char** argv) {
	int i;
	for(i = 0; i < sizeof(commands) / sizeof(struct command); i++)
		if(strcmp(commands[i].name, argv[0]) == 0)
			exit(commands[i].handler(argv));


	exit(execvp(argv[0], argv));
}


static int do_command(char* cmd) {
	char* argv[255];
	char* s = cmd;
	char* p = NULL;
	int i = 0;

	while((p = strchr(s, ' '))) {
		int len = (int) p - (int) s;
			
		argv[i] = (char*) malloc(len);
		strncpy(argv[i], (const char*) s, len);

		i++;
	}

	int r;
	do_sync(do_exec, argv, r);
		
	return r;
}

static int shell() {
	reset();

	do {
		static char command[8192];
		char* p = (char*) command;
		memset(command, 0, sizeof(command));

		printf("$ ");

		char ch;
		do {
			*p++ = ch = fgetc(stdin);
			printf("%c", ch);
		} while(ch != '\n');

		p--;
		*p++ = 0;

		do_command(command);
	} while(1);
}


int main(int argc, char** argv) {
	if(argc < 2)
		return shell();

	int i;
	for(i = i; i < argc; i++) {
		if(argv[i][0] != '-')
			die(ERR_USAGE);		

		switch(argv[i][1]) {
			case 'c':
				if(++i < argc)
					return do_command(argv[i + 1]);
				else
					die(ERR_CMDARG);
				break;
			case 'h':
				die(ERR_HELP);
				break;
			case 'u':
				die(ERR_USAGE);
				break;
			default:
				die(ERR_ARG);
		}
	}

	return 0;
}
