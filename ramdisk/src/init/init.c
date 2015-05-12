#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>

#define CAIRO 0
#if CAIRO
#include <cairo/cairo.h>
#endif

static void die(int unused) {
	(void) unused;

	exit(-1);
}


int main(int argc, char** argv) {



	char* rootdev = (char*) getenv("ROOTDEV");
	char* fstype = (char*) getenv("ROOTFS");

	if(!rootdev || !fstype)
		die(printf("init: cannot found root device!\n"));
	else
		if(mount(rootdev, "/dev/root", fstype, 0, NULL) != 0)
			die(printf("init: cannot mount %s in \"/dev/root\" (%s) - %s\n", rootdev, fstype, strerror(errno)));
		
	
	symlink("/dev", "/dev/root/dev");
	symlink("/tmp", "/dev/root/tmp");
	symlink("/proc", "/dev/root/proc");

	chroot("/dev/root");


	sleep(2);
	printf("Hllo\n");


#if CAIRO
	cairo_t* cx;
	cairo_surface_t* sx = cairo_image_surface_create_for_data(0xfc000000, CAIRO_FORMAT_RGB24, 800, 600, 800 * 4);
	cx = cairo_create(sx);

	printf("CAIRO_STATUS: %d == %d\n", cairo_status(cx), CAIRO_STATUS_SUCCESS);
#endif


#if 0
	if(fork() == 0)
		execl("/bin/session", NULL);
	else
		wait(NULL);
#endif

	return 0;
}
