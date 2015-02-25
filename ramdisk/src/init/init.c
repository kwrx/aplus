
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
#define async(x, y)		clone((int (*)(void*)) x, NULL, CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND, (void*) y)

int load_module(char* filename) {
	char* args[2];
	args[0] = filename;
	args[1] = NULL;


	printf("init: loading %s\n", args[0]);
	exit(execve(args[0], args, environ));
}


int test0(void* unused) {
	printf("test0: %d\n", getpid());
	for(;;) sched_yield();
}


int main(int argc, char** argv) {
	DIR* d = opendir(MOD_PATH);
	assert(d);

	struct dirent* ent;
	while(ent = readdir(d)) {
		static char buf[255];
		memset(buf, 0, 255);

		sprintf(buf, MOD_PATH "/%s", ent->d_name);

		//async(load_module, buf);
	}

	closedir(d);

	printf("init: drivers loaded\n");

	int i;
	for(i = 0; i < 10; i++)
		async(test0, NULL);

	for(;;) {
		int t0 = time(NULL);
		double cs = clock();
		while(t0 == time(NULL))
			;//sched_yield();
	
		double ce = clock();
		printf("init: cpu usage %g%%\n", (ce - cs) / CLOCKS_PER_SEC * 100.0);
	}

	return 0;
}
