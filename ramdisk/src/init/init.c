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

#include <GL/gl.h>
#include <GL/osmesa.h>


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


	/*atk_set_gfx(atk_gfx_create(800, 600, 32, 0xFD000000));
	
	atk_gfx_set_bitmap(atk_bitmap_from_framebuffer());
	atk_gfx_set_color_argb(1.0f, 1.0f, 1.0f, 1.0f);
	
	atk_gfx_set_font("/dev/ramdisk/share/fonts/ubunt000.ttf");
	atk_gfx_set_font_size(14, 72);
	
	atk_gfx_text("#define ARGB_TO_V4F(c)		\n\
	{											\n\
		((float) ((c >> 24) & 0xFF)) / 0xFF, 	\n\
		((float) ((c >> 16) & 0xFF)) / 0xFF,	\n\
		((float) ((c >> 8) & 0xFF)) / 0xFF,		\n\
		((float) (c & 0xFF)) / 0xFF				\n\
	}", 0, 0);
	*/


	//OSMesaContext ctx = OSMesaCreateContext(OSMESA_ARGB, NULL);
	//OSMesaMakeCurrent(ctx, (void*) 0xFD000000, GL_UNSIGNED_BYTE, 800, 600);


	int* x = malloc(15005404);
	int* y = malloc(0x1000);
	int* z = malloc(0x1000000);
	int* w = malloc(0x1000);

	
	assert(y);
	assert(z);
	assert(w);
	assert(x);

	*x = *y = *z = *w = 0xFFFFFFFF;

	printf("x: %d; y: %d; z: %d; w: %d\n", *x, *y, *z, *w);
	
	free(w);
	free(z);
	free(y);	
	free(x);
	
	

/*
	for(;;) {
		glClear(GL_COLOR_BUFFER_BIT);
		printf("cleared\n");
		
		glBegin(GL_POLYGON);
		glColor3f(1, 0, 0); glVertex3f(-0.6, 0.75, 0.5);
		glColor3f(0, 1, 0); glVertex3f(0.6, -0.75, 0);
		glColor3f(0, 0, 1); glVertex3f(0, 0.75, 0);
		glEnd();

		printf("flush\n");

		glFlush();

		int i = 1 / 0;
	}
*/
	for(;;) 
		sched_yield();

	return 0;
}
