#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <dlfcn.h>
#include <string.h>


#define MOD_PATH		"/dev/ramdisk/mod"

static int initmod(void* dl) {
	int (*init) () = dlsym(dl, "init");
	assert(init);


	init();
	return 0;
}



int main(int argc, char** argv) {
	DIR* d = opendir(MOD_PATH);
	assert(d);

	struct dirent* ent;
	while(ent = readdir(d)) {
		static char buf[255];
		memset(buf, 0, 255);

		sprintf(buf, MOD_PATH "/%s", ent->d_name);

		void* dl = dlopen(buf, RTLD_NOW);
		assert(dl);

		assert(initmod(dl) == 0);
		exit(0);
	}

	closedir(d);

	

	for(;;) 
		sched_yield();

	return 0;
}
