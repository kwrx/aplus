#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <dlfcn.h>
#include <string.h>

#include <atk.h>

#define MOD_PATH		"/dev/ramdisk/mod"

static int initmod(void* dl) {
	int (*init) () = dlsym(dl, "init");
	assert(init);


	init();
	return 0;
}

int main(int argc, char** argv) {
	/*DIR* d = opendir(MOD_PATH);
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

	closedir(d);*/


	atk_color_t clr = ATK_COLOR_WHITE;
	clr[ATK_COLOR_A] = 0.1f;
	atk_gfx_set(atk_gfx_create(800, 600, 32, 0xFD000000));
	atk_gfx_fill_rectangle(atk_bitmap_from_framebuffer(), 100, 100, 300, 300, clr);
	atk_gfx_fill_rectangle(atk_bitmap_from_framebuffer(), 200, 200, 300, 300, clr);

	for(;;) 
		sched_yield();

	return 0;
}
