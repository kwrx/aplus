#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include <sys/times.h>
#include <time.h>

#include "../../../../src/include/aplus.h"

#include <pthread.h>


clock_t ec;
clock_t sc;


void load_modules(char** argv, char** env) {
	chdir("/ramdisk");
	DIR* rd = (DIR*) opendir("/ramdisk");
	if(!rd) {
		printf("init: could not open ramdisk path\n");
		_exit(-1);
	}
		
	struct dirent* ent;
	while(ent = (struct dirent*) readdir(rd)) {
		char* p = (char*) ((uint32_t)ent->d_name + strlen(ent->d_name) - 3);
		
		if(strcmp(p, ".km") == 0) {
			printf("init: loading %-60s", ent->d_name);

			int ret = execve(ent->d_name, argv, env);
			if(ret == 0)
				printf("[OK]\n");
			else
				printf("[%d]\n", ret);
		}
	}
	
	closedir(rd);
	chdir("/");
}


void load_system() {

}



int main(int argc, char** argv) {
	sc = clock();

	load_system();
	load_modules(argv, environ);

	ec = clock();

	printf("System loaded in %fs\n", (double)(ec - sc) / (double) CLOCKS_PER_SEC);

	return 0;
}