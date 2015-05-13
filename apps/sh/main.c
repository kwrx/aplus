#include <stdio.h>
#include <stdlib.h>

#include "sh.h"


static void die(int e) {
	printf("%s: %s\n", __PROGNAME, __errors[e]);
	exit(1);
}


static int shell() {
	/* TODO */
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
					return do_command(&argv[i + 1]);
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
