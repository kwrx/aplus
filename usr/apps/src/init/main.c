#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include <sys/times.h>
#include <time.h>



clock_t ec;
clock_t sc;

#define _b __asm__ ("int $3");

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
		
		if(strcmp(p, ".km") == 0)
			execve(ent->d_name, argv, env);
	}
	
	closedir(rd);
	chdir("/");
}


void load_system() {
	
}

int main(int argc, char** argv) {
	load_modules(argv, environ);
	load_system();
	
	return 0;
}