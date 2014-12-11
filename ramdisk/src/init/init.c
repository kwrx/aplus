#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <dlfcn.h>
#include <string.h>

#include <atk.h>
#include <atk/bitmap.h>

#include <cairo/cairo.h>

#include <png.h>


#define MOD_PATH		"/dev/ramdisk/mod"

static int initmod(void* dl) {
	int (*init) () = dlsym(dl, "init");
	assert(init);


	init();
	return 0;
}

static void readpng(png_structp png, void* buf, int size) {
	int fd = png_get_io_ptr(png);
	read(fd, buf, size);
}

#define RGB_TO_V4F(c)							\
	{											\
		((float) ((c >> 24) & 0xFF)) / 0xFF,	\
		((float) ((c >> 16) & 0xFF)) / 0xFF,	\
		((float) ((c >> 8) & 0xFF)) / 0xFF,		\
		((float) (c & 0xFF)) / 0xFF				\
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


	cairo_surface_t* surface = cairo_image_surface_create_for_data(0xFD000000, CAIRO_FORMAT_ARGB32, 800, 600, 800 * 4);
	cairo_t* cr = cairo_create(surface);

	cairo_set_line_width(cr, 0.1);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_rectangle(cr, 0.25, 0.25, 0.5, 0.5);
	cairo_stroke(cr); 

	for(;;) 
		sched_yield();

	return 0;
}
