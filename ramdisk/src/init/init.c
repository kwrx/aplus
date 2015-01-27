#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <aplus/elf.h>


#define MOD_PATH		"/dev/ramdisk/mod"


void load_module(char* filename) {
	int fd = open(filename, O_RDONLY, 0644);
	if(fd < 0) {
		printf("init: cannot open module %s: %s\n", filename, strerror(errno));
		return;
	}

	lseek(fd, 0, SEEK_END);
	int sz = lseek(fd, 0, SEEK_CUR);
	lseek(fd, 0, SEEK_SET);

	void* image = (void*) malloc(sz);
	read(fd, image, sz);
	close(fd);


	elf_module_t elf;
	if(elf_load_module(&elf, image, sz, "init") != 0) {
		printf("init: cannot load module %s: %s\n", filename, strerror(errno));
		return;
	}

	for(;;);
	

	if(elf.start)
		((void (*) ()) elf.start) ();
	else
		printf("init: cannot load entry point for %s\n", filename);
}


int main(int argc, char** argv) {
	DIR* d = opendir(MOD_PATH);
	assert(d);

	struct dirent* ent;
	while(ent = readdir(d)) {
		static char buf[255];
		memset(buf, 0, 255);

		sprintf(buf, MOD_PATH "/%s", ent->d_name);

		load_module(buf);
	}

	closedir(d);

	

	for(;;) 
		sched_yield();

	return 0;
}
