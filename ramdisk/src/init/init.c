
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/times.h>
#include <aplus/elf.h>


#include <sched.h>


#define MOD_PATH		"/dev/ramdisk/mod"

int load_module(char* filename) {
	char* args[2];
	args[0] = filename;
	args[1] = NULL;


	printf("init: loading %s\n", args[0]);
	if(execve(args[0], args, environ) != 0)
		printf("init: failed to execute module %s - %s\n", filename, strerror(errno));

	exit(1);
}



int main(int argc, char** argv) {

	int ret = fork();
	printf("\n\n\n\n\n\nfork() -> %d from %d\n", ret, getpid());
	for(;;);

	DIR* d = opendir(MOD_PATH);
	assert(d);

	struct dirent* ent;
	while(ent = readdir(d)) {
		static char buf[255];
		memset(buf, 0, 255);

		sprintf(buf, MOD_PATH "/%s", ent->d_name);

		clone((int (*)(void*)) load_module, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_PARENT, (void*) buf);
		/*
			if(fork())
				load_module(buf);
		*/
	}

	closedir(d);

	printf("init: drivers loaded\n");

	for(;;) sched_yield();
	return 0;
}
