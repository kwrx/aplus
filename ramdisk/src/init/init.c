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
	int (*init) () = 0;//dlsym(dl, "init");
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

	atk_t atk;
	ATK_ASSERT(atk_main(&atk));
	
/*
	atk_window(&atk, &wnd, "Hello", 10, 10, 200, 200, ATK_WINDOW_HIDDEN);
	atk_window_destroy(&atk, &wnd);
	atk_window_show(&atk, &wnd);
	atk_window_hide(&atk, &wnd);
	atk_window_get_surface(&atk, &wnd);
	atk_window_get_renderer(&atk, &wnd);
	atk_window_get_context(&atk, &wnd);
	atk_window_get_flags(&atk, &wnd, &flags);
	atk_window_set_flags(&atk, &wnd, flags);
	atk_window_resize(&atk, &wnd);
	atk_window_move(&atk, &wnd);
	atk_window_add_widget(&atk, &wnd, &widget)
	atk_window_remove_widget(&atk, &wnd, &widget)
	atk_run(&atk);
*/

	atk_image_t* image;
	atk_load_image(&atk, &image, "/dev/ramdisk/share/images/test.png");

	atk_rect_t R;
	R.x = 0;
	R.y = 0;
	R.w = 500;
	R.h = 500;
	

	for(;;) 
		sched_yield();

	return 0;
}
