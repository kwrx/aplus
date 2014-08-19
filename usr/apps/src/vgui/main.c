


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <aplus/ioctl.h>


#include "vgui.h"
#include <kx.h>


void read_config(char* vmode) {
	videoconf.width = VGUI_DEFAULT_WIDTH;
	videoconf.height = VGUI_DEFAULT_HEIGHT;
	videoconf.bpp = VGUI_DEFAULT_BPP;	

	if(vmode) {
		sscanf(vmode, "%dx%dx%d", &videoconf.width, &videoconf.height, &videoconf.bpp);
	} else {
		int vd = open(VIDEOCONFIG_PATH, O_RDONLY, 0644);
		if(vd < 0) {
			warn("Could not open video configuration");
			return;
		}

		char* buf = malloc(BUFSIZ);
		read(vd, buf, 1024);
		close(vd);
	
		sscanf(buf, "%dx%dx%d", &videoconf.width, &videoconf.height, &videoconf.bpp);
		free(buf);
	}
}


static int vgui_exit() {
	int fd = open("/dev/tty0", O_RDONLY, 0644);
	if(fd < 0)
		return;
	
	ioctl(fd, IOCTL_TTY_RESET, 0);
	ioctl(fd, IOCTL_TTY_CLEAR, 0);
	close(fd);

	return 0;
}


int main(int argc, char** argv) {
	if(argc > 1) {
		for(int i = 1; i < argc; i++) {
			if(strcmp(argv[i], "--exit") == 0)
				return vgui_exit();

			if(strcmp(argv[i], "--vmode") == 0)
				read_config(argv[i + 1]);
		}
	} else
		read_config(NULL);


	dprintf(STDERR_FILENO, "Loading video mode: %dx%dx%d\n", videoconf.width, videoconf.height, videoconf.bpp);
	

	int fd = open("/dev/fb0", O_RDONLY, 0644);
	if(fd < 0)
		error("Could not open framebuffer device");
		
		
	exit_on_error(ioctl(fd, IOCTL_FB_DISABLE, 0));
	exit_on_error(ioctl(fd, IOCTL_FB_SETWIDTH, &videoconf.width));
	exit_on_error(ioctl(fd, IOCTL_FB_SETHEIGHT, &videoconf.height));
	exit_on_error(ioctl(fd, IOCTL_FB_SETBPP, &videoconf.bpp));
	exit_on_error(ioctl(fd, IOCTL_FB_ENABLE, 0));
	exit_on_error(ioctl(fd, IOCTL_FB_GETLFB, &videoconf.framebuff));

	close(fd);

	kx_context_t* kx = kx_createcontext(videoconf.width, videoconf.height, videoconf.framebuff);
	kx_setcontext(kx);
	kx_fillbox(100, 100, 300, 200, 0x80FFFFFF); 
	

	return 0;
}