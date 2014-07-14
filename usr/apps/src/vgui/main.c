

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <aplus/ioctl.h>


#include "vgui.h"

static void read_config() {
	int vd = open(VIDEOCONFIG_PATH, O_RDONLY, 0644);
	if(vd < 0) {
		warn("Could not open video configuration");
		
		videoconf.width = VGUI_DEFAULT_WIDTH;
		videoconf.height = VGUI_DEFAULT_HEIGHT;
		videoconf.bpp = VGUI_DEFAULT_BPP;
		
		return;
	}
		
	char* buf = malloc(BUFSIZ);
	read(vd, buf, 1024);
	close(vd);
	
	sscanf(buf, "%d %d %d", &videoconf.width, &videoconf.height, &videoconf.bpp);
	free(buf);
}


static void vgui_atexit() {
	int fd = open("/dev/tty0", O_RDONLY, 0644);
	if(fd < 0)
		return;
	
	ioctl(fd, IOCTL_TTY_RESET, 0);
	ioctl(fd, IOCTL_TTY_CLEAR, 0);
	close(fd);
}

int main(int argc, char** argv) {
	atexit(vgui_atexit);
	read_config();
	
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
}