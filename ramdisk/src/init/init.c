
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <aplus/elf.h>


#include <sched.h>


#define MOD_PATH		"/dev/ramdisk/mod"
#define async(x, y)		clone((int (*)(void*)) x, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND, (void*) y)

int load_module(char* filename) {
	char* args[2];
	args[0] = filename;
	args[1] = NULL;


	printf("loading %s\n", args[0]);

	exit(execve(args[0], args, environ));
}


int main(int argc, char** argv) {
	DIR* d = opendir(MOD_PATH);
	assert(d);

	struct dirent* ent;
	while(ent = readdir(d)) {
		static char buf[255];
		memset(buf, 0, 255);

		sprintf(buf, MOD_PATH "/%s", ent->d_name);

		async(load_module, buf);
	}

	closedir(d);

	

	for(;;) 
		sched_yield();

	return 0;
}
